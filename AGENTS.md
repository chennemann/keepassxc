# Downstream fork workflow

These instructions apply to the entire repository. This checkout is the
`chennemann/keepassxc` downstream fork, not an upstream KeePassXC release
checkout.

## Version control and branches

- This is a colocated Jujutsu repository. Use `jj` for repository operations;
  do not substitute equivalent `git` commands.
- Keep downstream work on the `fork` bookmark.
- `origin` must refer to `chennemann/keepassxc` and `upstream` to
  `keepassxreboot/keepassxc`.
- Upstream's mainline is `develop`, not `master`.
- GitHub Actions is intentionally disabled and `.github/workflows` must remain
  empty. Do not add or enable any hosted workflow unless the user explicitly
  reverses this decision.
- Fetch and rebase onto upstream `develop` locally when the user requests an
  update. Resolve and verify the rebase locally before pushing `fork`.

## Fork releases

Build fork releases locally so they do not consume GitHub Actions build
minutes. The canonical publisher is
`.github/scripts/publish_fork_release.ps1`; do not recreate its versioning,
tagging, or upload logic by hand.

From Git Bash, publish with:

```bash
powershell.exe -NoProfile -ExecutionPolicy Bypass \
  -File .github/scripts/publish_fork_release.ps1 -Bootstrap
```

Use `-ResolveOnly` when only the next version and target commit are needed.
Omit `-Bootstrap` after the required Qt toolchain is already available.

Before publishing, preserve these safeguards:

- The working copy must be clean and tree-equivalent to `fork`.
- Local `fork` must equal `fork@origin`.
- The target repository must be derived from the Jujutsu `origin` remote and
  must never be the upstream KeePassXC repository.
- Versions have the form `X.Y.Z-fork.N`, with monotonically increasing `N`.
- Compile and package successfully before creating or pushing a tag.
- Publish the portable Windows x64 ZIP and `SHA256SUMS` only after their hash
  has been verified.
- Treat an existing matching tag/release as an idempotent retry.
- Fork artifacts are unsigned; never describe them as official KeePassXC
  builds.

The publisher intentionally creates a portable ZIP. Do not install that ZIP by
overwriting `C:\Program Files\KeePassXC`; doing so corrupts Windows Installer's
ownership and upgrade records.

## Installing a fork release on this Windows machine

When the user asks to install the fork, install the same locally built version
that was published. Preserve `%APPDATA%\KeePassXC` and all database files.

1. Confirm the release ZIP checksum and identify the matching build directory
   at `out/fork-release/<version>`.
2. Prefer a locally produced WiX MSI from that build. Keep KeePassXC's upstream
   upgrade GUID so the MSI cleanly replaces the existing machine-wide version.
   The MSI version must remain numeric (for example `2.8.0`); the installed
   binaries must report the full fork version (for example
   `2.8.0-fork.1`).
3. Local release builds currently use `KPXC_FEATURE_DOCS=OFF`. The upstream WiX
   template still references two absent documentation files. Remove only the
   generated `GettingStartedShortcut` and `UserGuideShortcut` entries from the
   temporary WiX input before linking the MSI. Do not suppress the ICE67/ICE69
   missing-file errors, create broken shortcuts, or change the tracked
   upstream template merely to package one installation.
4. Keep this MSI local unless the user explicitly asks to publish it. It is
   unsigned.
5. Install via `msiexec` with `/norestart`; elevation requires a visible UAC
   consent prompt. If consent is canceled, report that the system is unchanged
   and retry only when the user asks.
6. After installation, verify all of the following:
   - `C:\Program Files\KeePassXC\keepassxc-cli.exe --version` reports the full
     `X.Y.Z-fork.N` version.
   - The uninstall registry entry reports the numeric MSI version and points
     to `C:\Program Files\KeePassXC`.
   - The previous MSI product registration is gone.
   - `C:\Program Files\KeePassXC\.portable` does not exist.
   - `%APPDATA%\KeePassXC` still exists when it existed before installation.

Do not silently fall back to a side-by-side portable installation: shortcuts,
file associations, autostart, and browser integration could continue launching
the old machine-wide build.

See `.github/FORK.md` for the user-facing overview of the fork automation.
