#!/usr/bin/env python3
"""Reindex writable maps using the shared edited styles, then relaunch on success.

recalculate_geom_index.py <resources_dir> <writable_dir> <generator_tool> [<app> <args>...]
Bundled World maps must be copied to the writable directory before this script runs.
"""

import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from tempfile import TemporaryDirectory

WORKERS = 8
EXCLUDE_NAMES = {"WorldCoasts.mwm", "WorldCoasts_migrate.mwm"}


def find_all_mwms(data_path):
    return sorted(
        {
            p.resolve()
            for p in Path(data_path).rglob("*.mwm")
            if p.name not in EXCLUDE_NAMES
        }
    )


def process_mwm(generator_tool, mwm, resources_dir, writable_dir):
    print(f"Processing {mwm}", flush=True)
    # Duplicate country names across versions must never share an index temporary file.
    with TemporaryDirectory(prefix="designer-index-") as tmp:
        subprocess.run(
            [
                generator_tool,
                f"--data_path={writable_dir}",
                f"--user_resource_path={resources_dir}",
                f"--mwm_file={mwm}",
                "--generate_index=true",
                f"--intermediate_data_path={tmp}{os.sep}",
            ],
            check=True,
        )


def main(argv=None):
    args = sys.argv[1:] if argv is None else argv
    if len(args) < 3:
        print(__doc__, file=sys.stderr)
        return 1
    resources_dir, writable_dir, generator_tool, *relaunch = args
    try:
        mwms = find_all_mwms(writable_dir)
        with ThreadPoolExecutor(max_workers=WORKERS) as executor:
            list(
                executor.map(
                    lambda mwm: process_mwm(
                        generator_tool, mwm, resources_dir, writable_dir
                    ),
                    mwms,
                )
            )
        if relaunch:
            # The caller closes our output pipes when we exit; the relaunched app needs independent streams.
            devnull = subprocess.DEVNULL
            subprocess.Popen(relaunch, stdin=devnull, stdout=devnull, stderr=devnull)
    except (OSError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
