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
                (writable / version / "Country.mwm").touch()
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
                self.assertTrue(any(a.startswith("--mwm_file=") for a in args))
            self.assertTrue(all(not Path(p).exists() for p in temporaries))
            self.assertTrue((resources / "World.mwm").is_file())

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
