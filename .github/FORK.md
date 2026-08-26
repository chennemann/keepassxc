# KeePassXC downstream fork automation

This repository keeps downstream changes on the `fork` branch. KeePassXC does
not have a `master` branch; its upstream mainline is
`keepassxreboot/keepassxc:develop`, so that is the rebase base used here.

GitHub Actions is disabled for this fork and the repository contains no
workflow definitions. Upstream updates are fetched, rebased, and verified
locally before `fork` is pushed. This keeps rebases and builds off GitHub's
hosted runners.

Releases are built locally to avoid spending hosted GitHub Actions minutes. On
Windows, run the following from Git Bash (the first build can bootstrap a local
Qt installation):

```bash
powershell.exe -NoProfile -ExecutionPolicy Bypass \
  -File .github/scripts/publish_fork_release.ps1 -Bootstrap
```

The publisher verifies that the working tree matches the remote `fork` branch,
builds an unsigned portable Windows x64 ZIP, and only then creates the immutable
tag and GitHub release. Versions use the numeric KeePassXC version from
`CMakeLists.txt` plus a monotonically increasing suffix, for example
`2.8.0-fork.1`. A tag without a release is treated as an idempotent retry.

Use `-ResolveOnly` to inspect the next version without building or publishing.

Fork releases are not official KeePassXC builds and are intentionally unsigned.
