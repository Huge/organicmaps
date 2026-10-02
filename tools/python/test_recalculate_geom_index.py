import importlib.util
from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory
import threading
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "recalculate", Path(__file__).with_name("recalculate_geom_index.py")
)
recalculate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recalculate)


class RecalculateIndexTests(unittest.TestCase):
    def test_duplicate_names_get_unique_temporaries_and_shared_styles(self):
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            resources, writable = root / "bundle", root / "data"
            resources.mkdir()
            writable.mkdir()
            (resources / "World.mwm").touch()
            for version in ("1", "2"):
                (writable / version).mkdir()
                (writable / version / "Country with spaces.mwm").touch()
            (writable / "WorldCoasts.mwm").touch()
            barrier = threading.Barrier(2)
            temporaries = []
            calls = []

            def run(args, **kwargs):
                calls.append(args)
                path = next(
                    a.split("=", 1)[1]
                    for a in args
                    if a.startswith("--intermediate_data_path=")
                )
                self.assertTrue(Path(path).is_dir())
                output = next(
                    a.split("=", 1)[1] for a in args if a.startswith("--output=")
                )
                self.assertTrue((Path(path) / output).parent.is_dir())
                self.assertTrue((writable / (output + ".mwm")).is_file())
                temporaries.append(path)
                barrier.wait(timeout=5)
                return subprocess.CompletedProcess(args, 0)

            with (
                patch.object(recalculate.subprocess, "run", side_effect=run),
                patch.object(recalculate.subprocess, "Popen") as relaunch,
            ):
                self.assertEqual(
                    recalculate.main(
                        [
                            str(resources),
                            str(writable),
                            "generator",
                            "app",
                            "--designer=style",
                        ]
                    ),
                    0,
                )
                relaunch.assert_called_once()
            self.assertEqual(len(calls), 2)
            self.assertEqual(len(set(temporaries)), 2)
            for args in calls:
                self.assertIn(f"--data_path={writable}", args)
                self.assertIn(f"--user_resource_path={resources}", args)
            self.assertEqual(
                {a for args in calls for a in args if a.startswith("--output=")},
                {
                    f"--output={Path(version) / 'Country with spaces'}"
                    for version in ("1", "2")
                },
            )
            self.assertTrue(all(not Path(p).exists() for p in temporaries))
            self.assertTrue((resources / "World.mwm").is_file())

    def test_symlinked_writable_root_keeps_relative_paths_and_deduplicates_maps(self):
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            writable = root / "data"
            writable.mkdir()
            source = root / "Country.mwm"
            source.touch()
            alias = root / "data-link"
            try:
                alias.symlink_to(writable, target_is_directory=True)
                for name in ("Country.mwm", "Country-copy.mwm"):
                    (writable / name).symlink_to(source)
            except (OSError, NotImplementedError) as error:
                self.skipTest(f"Cannot create symlinks: {error}")
            with patch.object(recalculate.subprocess, "run") as run:
                self.assertEqual(recalculate.main([tmp, str(alias), "generator"]), 0)
            run.assert_called_once()
            args = run.call_args.args[0]
            output = next(a.split("=", 1)[1] for a in args if a.startswith("--output="))
            self.assertFalse(Path(output).is_absolute())
            self.assertNotIn("..", Path(output).parts)
            self.assertEqual((alias / (output + ".mwm")).resolve(), source.resolve())

    def test_failure_does_not_relaunch(self):
        for error in (
            FileNotFoundError("missing generator"),
            subprocess.CalledProcessError(2, "generator"),
        ):
            with self.subTest(error=error), TemporaryDirectory() as tmp:
                root = Path(tmp)
                (root / "Country.mwm").touch()
                with (
                    patch.object(recalculate.subprocess, "run", side_effect=error),
                    patch.object(recalculate.subprocess, "Popen") as relaunch,
                ):
                    self.assertEqual(
                        recalculate.main([tmp, tmp, "generator", "app"]), 1
                    )
                    relaunch.assert_not_called()


if __name__ == "__main__":
    unittest.main()
