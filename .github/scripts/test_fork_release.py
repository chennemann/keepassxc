#!/usr/bin/env python3

import tempfile
import unittest
from pathlib import Path

from fork_release import read_base_version, resolve_release


class ForkReleaseTest(unittest.TestCase):
    def test_first_fork_release(self) -> None:
        result = resolve_release("2.8.0", ["2.7.12", "latest"], [])
        self.assertEqual(result.version, "2.8.0-fork.1")
        self.assertFalse(result.already_tagged)

    def test_increments_only_the_current_upstream_base(self) -> None:
        result = resolve_release(
            "2.8.0",
            ["2.7.12-fork.20", "2.8.0-fork.1", "2.8.0-fork.3"],
            [],
        )
        self.assertEqual(result.version, "2.8.0-fork.4")

    def test_reuses_tag_on_the_same_commit(self) -> None:
        result = resolve_release(
            "2.8.0",
            ["2.8.0-fork.1", "2.8.0-fork.2"],
            ["2.8.0-fork.2"],
        )
        self.assertEqual(result.version, "2.8.0-fork.2")
        self.assertTrue(result.already_tagged)

    def test_rejects_target_tag_for_another_base(self) -> None:
        with self.assertRaisesRegex(ValueError, "different KeePassXC base"):
            resolve_release("2.8.0", ["2.7.12-fork.1"], ["2.7.12-fork.1"])

    def test_reads_cmake_version(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            cmake = Path(directory) / "CMakeLists.txt"
            cmake.write_text(
                "\n".join(
                    [
                        'set(KEEPASSXC_VERSION_MAJOR "2")',
                        'set(KEEPASSXC_VERSION_MINOR "8")',
                        'set(KEEPASSXC_VERSION_PATCH "0")',
                    ]
                ),
                encoding="utf-8",
            )
            self.assertEqual(read_base_version(cmake), "2.8.0")


if __name__ == "__main__":
    unittest.main()
