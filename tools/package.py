"""Builds a release of the mod.

    python tools/package.py v0.1.0 [--no-build]

1. build.ps1 (dist\\ holds exactly this build)
2. out\\SteinsGateAccess-<tag>.zip, laid out like the game folder (the online installer,
   ..\\installer SteinsGateAccessibilityInstaller.exe, extracts it into the game folder):
       dinput8.dll                      (the mod, next to Game.exe)
       SteinsGateAccess\\SGALaunch.exe   (starts the game through the junction)
       SteinsGateAccess\\SpeechHost.exe + prism.dll
       SteinsGateAccess\\data\\..., LICENSES\\..., README.md
   and out\\SteinsGateAccess-<tag>.map, the mod's map for crash offsets of that release.
"""
import argparse
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIST = ROOT / "dist"
OUT = ROOT / "out"
# Files the installer checks for after extracting; a release without one of them is broken.
REQUIRED = [
    "dinput8.dll",
    "SteinsGateAccess/SGALaunch.exe",
    "SteinsGateAccess/SpeechHost.exe",
    "SteinsGateAccess/prism.dll",
    "SteinsGateAccess/README.md",
    "SteinsGateAccess/data/bg.tsv",
    "SteinsGateAccess/data/charset.utf8",
    "SteinsGateAccess/data/lang/en.txt",
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tag", help="release tag, e.g. v0.1.0")
    ap.add_argument("--no-build", action="store_true", help="package dist\\ as it is")
    args = ap.parse_args()
    if not re.fullmatch(r"v\d+\.\d+\.\d+", args.tag):
        sys.exit("tag must look like v1.2.3")

    if not args.no_build:
        subprocess.run(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
                        "-File", str(ROOT / "build.ps1")], check=True)

    files = sorted(f for f in DIST.rglob("*") if f.is_file())
    names = {f.relative_to(DIST).as_posix() for f in files}
    missing = [n for n in REQUIRED if n not in names]
    if missing:
        sys.exit("missing from dist: " + ", ".join(missing))

    OUT.mkdir(exist_ok=True)
    zip_path = OUT / f"SteinsGateAccess-{args.tag}.zip"
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        for f in files:
            z.write(f, f.relative_to(DIST).as_posix())
    shutil.copyfile(ROOT / "build" / "x86" / "dinput8.map", OUT / f"SteinsGateAccess-{args.tag}.map")
    print(f"{zip_path} ({zip_path.stat().st_size:,} bytes, {len(files)} files)")


if __name__ == "__main__":
    main()
