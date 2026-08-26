#!/usr/bin/env python3
"""Resolve deterministic KeePassXC fork release versions from Git tags."""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path


VERSION_PART = r"(0|[1-9][0-9]*)"
FORK_VERSION_RE = re.compile(
    rf"^(?P<base>{VERSION_PART}[.]{VERSION_PART}[.]{VERSION_PART})-fork[.](?P<revision>[1-9][0-9]*)$"
)


@dataclass(frozen=True)
class ReleaseResolution:
    version: str
    tag: str
    already_tagged: bool


def read_base_version(cmake_file: Path) -> str:
    text = cmake_file.read_text(encoding="utf-8")
    parts: list[str] = []
    for name in ("MAJOR", "MINOR", "PATCH"):
        match = re.search(
            rf'^set\(KEEPASSXC_VERSION_{name} "(0|[1-9][0-9]*)"\)$',
            text,
            re.MULTILINE,
        )
        if match is None:
            raise ValueError(f"Could not read KEEPASSXC_VERSION_{name} from {cmake_file}")
        parts.append(match.group(1))
    return ".".join(parts)


def read_tags(path: Path) -> list[str]:
    return [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def resolve_release(base: str, existing_tags: list[str], target_tags: list[str]) -> ReleaseResolution:
    parsed_existing = [match for tag in existing_tags if (match := FORK_VERSION_RE.fullmatch(tag))]
    parsed_target = [match for tag in target_tags if (match := FORK_VERSION_RE.fullmatch(tag))]

    mismatched_target = [match.group(0) for match in parsed_target if match.group("base") != base]
    if mismatched_target:
        raise ValueError(
            f"Commit is already tagged for a different KeePassXC base: {', '.join(mismatched_target)}"
        )

    matching_target = [match for match in parsed_target if match.group("base") == base]
    if len(matching_target) > 1:
        raise ValueError(f"Commit has multiple fork release tags for {base}")
    if matching_target:
        version = matching_target[0].group(0)
        return ReleaseResolution(version=version, tag=version, already_tagged=True)

    revisions = [
        int(match.group("revision"))
        for match in parsed_existing
        if match.group("base") == base
    ]
    revision = max(revisions, default=0) + 1
    version = f"{base}-fork.{revision}"
    return ReleaseResolution(version=version, tag=version, already_tagged=False)


def write_github_output(resolution: ReleaseResolution, output_file: Path) -> None:
    with output_file.open("a", encoding="utf-8", newline="\n") as output:
        output.write(f"version={resolution.version}\n")
        output.write(f"tag={resolution.tag}\n")
        output.write(f"already_tagged={'true' if resolution.already_tagged else 'false'}\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", type=Path, default=Path("CMakeLists.txt"))
    parser.add_argument("--existing-tags", type=Path, required=True)
    parser.add_argument("--target-tags", type=Path, required=True)
    parser.add_argument("--github-output", type=Path)
    args = parser.parse_args()

    resolution = resolve_release(
        read_base_version(args.cmake),
        read_tags(args.existing_tags),
        read_tags(args.target_tags),
    )
    if args.github_output:
        write_github_output(resolution, args.github_output)
    else:
        print(resolution.version)


if __name__ == "__main__":
    main()
