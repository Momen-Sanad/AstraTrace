#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path


RULES = [
    ("src/core", ["scene/", "render/", "io/", "app/", "platform/"]),
    ("src/scene", ["render/", "app/", "platform/"]),
    ("src/io", ["render/", "app/", "platform/"]),
    ("src/render", ["app/", "platform/", "io/"]),
]


def iter_source_files(root: Path):
    for ext in ("*.hpp", "*.h", "*.cpp", "*.cc", "*.cxx"):
        yield from root.rglob(ext)


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    include_re = re.compile(r'^\s*#\s*include\s*"([^"]+)"')
    violations: list[str] = []

    for relative_root, forbidden_prefixes in RULES:
        scan_root = repo_root / relative_root
        if not scan_root.exists():
            continue

        for path in iter_source_files(scan_root):
            rel = path.relative_to(repo_root).as_posix()
            for line_no, line in enumerate(path.read_text(encoding="utf-8", errors="ignore").splitlines(), start=1):
                match = include_re.match(line)
                if not match:
                    continue
                include_path = match.group(1)
                if any(include_path.startswith(prefix) for prefix in forbidden_prefixes):
                    violations.append(f"{rel}:{line_no}: forbidden include '{include_path}'")

    if violations:
        print("Include boundary violations detected:")
        for violation in violations:
            print(f"  - {violation}")
        return 1

    print("Include boundary checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
