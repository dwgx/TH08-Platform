"""Copy th08-decomp/ and helpers into th08-multiplayer/ monorepo.
Excludes build artifacts, VS2002 downloads, copyrighted binaries, logs.
"""
import os
import shutil
from pathlib import Path

SRC_REPO = Path("D:/Project/THGlobal/th08-decomp")
DST_REPO = Path("D:/Project/THGlobal/TH08-Platform")
TOOLS_SRC = Path("D:/Project/THGlobal")

# Directories under th08-decomp/ that we EXCLUDE completely from the monorepo
EXCLUDED_DIRS = {
    ".git",
    "build",
}
# Pairs (relative path from SRC_REPO) that we EXCLUDE
EXCLUDED_PATHS = {
    Path("scripts/prefix"),
    Path("scripts/dls"),
    Path("resources/th08.exe"),
    Path("resources/thbgm.dat"),
    Path("resources/th08.dat"),
}
# File name patterns to exclude
EXCLUDED_FILE_PATTERNS = [".log", ".obj"]
EXCLUDED_FILE_EXACT = {"diff_report.json", "diff_report_post_b3.json"}
EXCLUDED_FILE_PREFIX = ["worksheet"]  # worksheets handled separately


def should_skip(p: Path, rel: Path) -> bool:
    """Return True if this path should not be copied."""
    parts = rel.parts
    # Excluded top-level dirs
    if parts and parts[0] in EXCLUDED_DIRS:
        return True
    # Excluded specific subpaths
    for bad in EXCLUDED_PATHS:
        if rel == bad or bad in rel.parents:
            return True
    if p.is_file():
        name = p.name
        if name in EXCLUDED_FILE_EXACT:
            return True
        for pat in EXCLUDED_FILE_PATTERNS:
            if name.endswith(pat):
                return True
        for pre in EXCLUDED_FILE_PREFIX:
            if name.startswith(pre):
                return True
    return False


def mirror_tree(src: Path, dst: Path, base: Path):
    for item in src.iterdir():
        rel = item.relative_to(base)
        if should_skip(item, rel):
            continue
        target = dst / item.name
        if item.is_dir():
            target.mkdir(parents=True, exist_ok=True)
            mirror_tree(item, target, base)
        else:
            shutil.copy2(item, target)


def main():
    print(f"Copying {SRC_REPO} -> {DST_REPO}/game/")
    game_dir = DST_REPO / "game"
    game_dir.mkdir(parents=True, exist_ok=True)
    mirror_tree(SRC_REPO, game_dir, SRC_REPO)

    # Tools
    print("Copying tools/ helpers")
    tools_dir = DST_REPO / "tools"
    tools_dir.mkdir(parents=True, exist_ok=True)
    for name in ["ida_helper.py",
                 "batch_analyze.py", "batch_analyze_v2.py",
                 "batch_analyze_v3.py", "batch_analyze_v4.py",
                 "batch_analyze_v5.py",
                 "copy_to_monorepo.py"]:
        src = TOOLS_SRC / name
        if src.exists():
            shutil.copy2(src, tools_dir / name)

    # Worksheets go into docs/worksheets/ not game/
    print("Moving worksheets to docs/worksheets/")
    ws_dir = DST_REPO / "docs" / "worksheets"
    ws_dir.mkdir(parents=True, exist_ok=True)
    for p in SRC_REPO.glob("worksheet*.md"):
        shutil.copy2(p, ws_dir / p.name)

    # Quick size audit
    def du(path):
        total = 0
        for root, dirs, files in os.walk(path):
            for f in files:
                try:
                    total += os.path.getsize(os.path.join(root, f))
                except OSError:
                    pass
        return total

    for sub in ["game", "dll", "tools", "docs"]:
        sz = du(DST_REPO / sub)
        print(f"  {sub}: {sz / 1e6:.2f} MB")

    # File count
    print("Done.")


if __name__ == "__main__":
    main()
