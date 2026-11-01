#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Prepare a local BLE release, matching source and uploadable installer site; never publish or flash (Saadet's
tools/prepare_public_release.py, for Melodee Remote).

  tools/prepare_public_release.py 0.1.0 [--out DIR] [--no-build] [--checks FILE.json]
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import zipfile

from make_site import main as make_site, pinned_base, release_product
from release_support import NAME, make_source_archive, sha256, validate_build, source_state

ROOT = Path(__file__).resolve().parents[1]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("version")
    parser.add_argument("--out", type=Path, help="new output directory (default: build/releases/VERSION)")
    parser.add_argument("--no-build", action="store_true", help="reuse a matching versioned build and receipt")
    parser.add_argument("--checks", type=Path, help="completed verification results JSON to carry with the release")
    args = parser.parse_args(argv)
    product = release_product(args.version)
    if source_state(ROOT)["dirty"]:
        raise SystemExit("Commit all source changes before preparing a public release.")
    output = (args.out or ROOT / "build/releases" / args.version).resolve()
    if output.exists():
        raise SystemExit(f"{output} already exists; choose a new release output directory.")
    if not args.no_build:
        env = dict(os.environ, PYTHON=sys.executable)
        subprocess.run([str(ROOT / "build.sh"), "--release", args.version], cwd=ROOT, env=env, check=True)
    package = ROOT / "app/build" / f"{NAME}-{args.version}.fwsc"
    receipt = validate_build(ROOT, package, args.version)
    if receipt["product"] != product:
        raise SystemExit("Release identity differs from the selected version.")
    _, base = pinned_base()
    checks = None
    if args.checks:
        checks = json.loads(args.checks.read_text())
        if not checks or any(item.get("exit_code") != 0 for item in checks):
            raise SystemExit("Release verification contains failed or incomplete checks.")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=f".{NAME}-release-", dir=output.parent) as temporary:
        staging = Path(temporary) / "release"
        staging.mkdir()
        copied = staging / package.name
        shutil.copyfile(package, copied)
        build_info = staging / "BUILD_INFO.json"
        build_info.write_text(json.dumps(receipt, indent=2) + "\n")
        source = staging / f"{NAME}-{args.version}-source.zip"
        make_source_archive(ROOT, args.version, source, receipt, base)
        make_site([str(copied), args.version, str(staging / "site"),
                   "--source", str(source), "--build-info", str(build_info)])
        for name in ("LICENSE", "LICENSING.md"):
            shutil.copyfile(ROOT / name, staging / name)
        shutil.copytree(staging / "site/firmware/LICENSES", staging / "LICENSES")
        if checks:
            (staging / "VERIFICATION.json").write_text(json.dumps(checks, indent=2) + "\n")
        site_zip = staging / f"{NAME}-{args.version}-site.zip"
        with zipfile.ZipFile(site_zip, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for path in sorted((staging / "site").rglob("*")):
                if path.is_file():
                    archive.write(path, path.relative_to(staging / "site"))
        release = {"version": args.version, "product": product, "commit": receipt["source"]["commit"],
                   "firmware": copied.name, "source": source.name, "site": site_zip.name,
                   "published": False, "installed": False}
        (staging / "RELEASE_INFO.json").write_text(json.dumps(release, indent=2) + "\n")
        (staging / "SHA256SUMS").write_text("".join(
            f"{sha256(path.read_bytes())}  {path.relative_to(staging).as_posix()}\n"
            for path in sorted(staging.rglob("*")) if path.is_file() and path != staging / "SHA256SUMS"))
        validate_build(ROOT, package, args.version)
        shutil.move(str(staging), output)
    print(f"Prepared Melodee Remote {args.version}: {output}")
    return output


if __name__ == "__main__":
    main()
