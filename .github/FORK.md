# KeePassXC downstream fork automation

This repository keeps downstream changes on the `fork` branch. KeePassXC does
not have a `master` branch; its upstream mainline is
`keepassxreboot/keepassxc:develop`, so that is the rebase base used here.

The `Fork upstream rebase` workflow runs every three hours. It fetches the
upstream `develop` branch and rebases `fork` onto it. The update uses
`--force-with-lease`, and a conflict fails the workflow without changing the
remote branch. It never starts a hosted build.

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
