[CmdletBinding()]
param(
    # Retained for command compatibility; vcpkg installs the Qt 5 toolchain automatically.
    [switch]$Bootstrap,
    [switch]$ResolveOnly,
    [int]$Parallelism = [Math]::Max(1, [Environment]::ProcessorCount - 1)
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$releaseRoot = Join-Path $repoRoot "out\fork-release"

function Invoke-Checked {
    param(
        [Parameter(Mandatory)] [string]$Command,
        [Parameter()] [string[]]$Arguments = @(),
        [Parameter()] [string]$WorkingDirectory = $repoRoot
    )

    Push-Location $WorkingDirectory
    try {
        & $Command @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$Command exited with code $LASTEXITCODE."
        }
    } finally {
        Pop-Location
    }
}

function Invoke-Captured {
    param(
        [Parameter(Mandatory)] [string]$Command,
        [Parameter()] [string[]]$Arguments = @(),
        [Parameter()] [string]$WorkingDirectory = $repoRoot
    )

    Push-Location $WorkingDirectory
    try {
        $output = & $Command @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "$Command exited with code $LASTEXITCODE."
        }
        return ($output | Out-String).Trim()
    } finally {
        Pop-Location
    }
}

function Require-Command {
    param([Parameter(Mandatory)] [string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $command) {
        throw "Required command '$Name' was not found on PATH."
    }
    return $command.Source
}

function Get-TagNames {
    param([string[]]$Arguments = @())

    $output = Invoke-Captured -Command "jj" -Arguments (@("tag", "list") + $Arguments)
    if (-not $output) {
        return @()
    }
    return @(
        $output -split "`r?`n" |
            ForEach-Object {
                if ($_ -match '^([^:\s][^:]*):') {
                    $Matches[1]
                }
            } |
            Where-Object { $_ } |
            Sort-Object -Unique
    )
}

function Find-VisualStudio {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "Visual Studio Installer's vswhere.exe was not found. Install Visual Studio 2022 Build Tools."
    }
    $installPath = Invoke-Captured -Command $vswhere -Arguments @(
        "-latest",
        "-products", "*",
        "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "Microsoft.VisualStudio.Component.VC.ATL", "Microsoft.VisualStudio.Component.VC.ATLMFC",
        "-property", "installationPath"
    )
    if (-not $installPath) {
        throw "Visual Studio 2022 with the x64 C++ build tools and ATL/MFC components was not found."
    }
    return $installPath
}

function Find-VcpkgRoot {
    param([Parameter(Mandatory)] [string]$VisualStudioRoot)

    $candidates = @(
        $env:VCPKG_ROOT,
        $env:VCPKG_INSTALLATION_ROOT,
        (Join-Path $VisualStudioRoot "VC\vcpkg"),
        "C:\vcpkg"
    ) | Where-Object { $_ }
    foreach ($candidate in $candidates) {
        $resolved = [System.IO.Path]::GetFullPath($candidate)
        if (Test-Path -LiteralPath (Join-Path $resolved "scripts\buildsystems\vcpkg.cmake")) {
            return $resolved
        }
    }
    throw "vcpkg was not found. Set VCPKG_ROOT or install it with Visual Studio."
}

foreach ($required in @("jj", "gh", "python", "cmake", "cpack")) {
    Require-Command -Name $required | Out-Null
}

Push-Location $repoRoot
try {
    $actualRoot = Invoke-Captured -Command "jj" -Arguments @("root")
    if ([System.IO.Path]::GetFullPath($actualRoot) -ne $repoRoot) {
        throw "Expected Jujutsu root '$repoRoot', found '$actualRoot'."
    }

    $treeDiff = Invoke-Captured -Command "jj" -Arguments @(
        "diff", "--from", "fork", "--to", "@", "--summary"
    )
    if ($treeDiff) {
        throw "The working-copy tree differs from fork. Commit or discard those changes before publishing.`n$treeDiff"
    }

    Invoke-Checked -Command "jj" -Arguments @("git", "fetch", "--remote", "origin")
    $forkCommit = Invoke-Captured -Command "jj" -Arguments @(
        "log", "-r", "fork", "--no-graph", "-T", "commit_id"
    )
    $remoteCommit = Invoke-Captured -Command "jj" -Arguments @(
        "log", "-r", "fork@origin", "--no-graph", "-T", "commit_id"
    )
    if ($forkCommit -ne $remoteCommit) {
        throw "Local fork ($forkCommit) does not match fork@origin ($remoteCommit)."
    }

    $existingTags = Get-TagNames
    $targetTags = Get-TagNames -Arguments @("-r", "fork")
    $existingTagsFile = [System.IO.Path]::GetTempFileName()
    $targetTagsFile = [System.IO.Path]::GetTempFileName()
    try {
        $existingTags | Set-Content -LiteralPath $existingTagsFile -Encoding ascii
        $targetTags | Set-Content -LiteralPath $targetTagsFile -Encoding ascii
        $version = Invoke-Captured -Command "python" -Arguments @(
            "-B", ".github/scripts/fork_release.py",
            "--existing-tags", $existingTagsFile,
            "--target-tags", $targetTagsFile
        )
    } finally {
        Remove-Item -LiteralPath $existingTagsFile -Force
        Remove-Item -LiteralPath $targetTagsFile -Force
    }

    $remoteList = Invoke-Captured -Command "jj" -Arguments @("git", "remote", "list")
    $originLine = @($remoteList -split "`r?`n" | Where-Object { $_ -match '^origin\s+' })
    if ($originLine.Count -ne 1 -or $originLine[0] -notmatch '^origin\s+(?<url>\S+)$') {
        throw "Could not resolve exactly one Jujutsu origin remote."
    }
    $originUrl = $Matches["url"]
    if ($originUrl -notmatch 'github[.]com[/:](?<repository>[^/\s]+/[^/\s]+?)(?:[.]git)?$') {
        throw "Jujutsu origin is not a supported GitHub URL: $originUrl"
    }
    $repository = $Matches["repository"]
    if ($repository -eq "keepassxreboot/keepassxc") {
        throw "Refusing to publish a fork release to the upstream repository."
    }
    $verifiedRepository = Invoke-Captured -Command "gh" -Arguments @(
        "repo", "view", $repository, "--json", "nameWithOwner", "--jq", ".nameWithOwner"
    )
    if ($verifiedRepository -ne $repository) {
        throw "GitHub resolved '$repository' as unexpected repository '$verifiedRepository'."
    }
    Write-Host "Fork commit: $forkCommit"
    Write-Host "Release:     $repository@$version"

    if ($ResolveOnly) {
        return
    }

    $previousErrorPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    & gh release view $version --repo $repository 1> $null 2> $null
    $releaseExists = $LASTEXITCODE -eq 0
    $ErrorActionPreference = $previousErrorPreference
    if ($releaseExists) {
        Write-Host "GitHub release $version already exists; nothing to do."
        return
    }

    $visualStudioRoot = Find-VisualStudio
    # Populate SDK library paths so the stable branch can locate WindowsApp.lib.
    Import-Module (Join-Path $visualStudioRoot "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
    Enter-VsDevShell -VsInstallPath $visualStudioRoot -SkipAutomaticLocation `
        -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null
    $vcpkgRoot = Find-VcpkgRoot -VisualStudioRoot $visualStudioRoot
    $buildDirectory = Join-Path $releaseRoot $version
    New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
    $toolchain = Join-Path $vcpkgRoot "scripts\buildsystems\vcpkg.cmake"
    # Qt 5 uses relative include paths that exceed MAX_PATH in deeply nested build trees.
    $vcpkgBuildRoot = Join-Path $env:LOCALAPPDATA "kpxc\buildtrees"
    Invoke-Checked -Command "cmake" -Arguments @(
        "-S", $repoRoot,
        "-B", $buildDirectory,
        "-G", "Visual Studio 17 2022",
        "-A", "x64",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
        "-DVCPKG_INSTALL_OPTIONS=--x-buildtrees-root=$vcpkgBuildRoot",
        "-DOVERRIDE_VERSION=$version",
        "-DKEEPASSXC_BUILD_TYPE=Release",
        "-DWITH_XC_DOCS=OFF",
        "-DWITH_TESTS=OFF",
        "-DWITH_GUI_TESTS=OFF",
        "-DWITH_XC_ALL=ON",
        "-DX_VCPKG_APPLOCAL_DEPS_INSTALL=ON"
    )
    Invoke-Checked -Command "cmake" -Arguments @(
        "--build", $buildDirectory,
        "--config", "Release",
        "--parallel", $Parallelism.ToString()
    )
    Invoke-Checked -Command "cpack" -Arguments @(
        "--config", (Join-Path $buildDirectory "CPackConfig.cmake"),
        "-C", "Release",
        "-G", "ZIP"
    ) -WorkingDirectory $buildDirectory

    $packages = @(Get-ChildItem -LiteralPath $buildDirectory -File -Filter "*.zip" | Where-Object {
        $_.Name -like "*$version*"
    })
    if ($packages.Count -ne 1) {
        throw "Expected exactly one ZIP containing $version, found $($packages.Count)."
    }
    $checksumFile = Join-Path $buildDirectory "SHA256SUMS"
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $packages[0].FullName).Hash.ToLowerInvariant()
    "$hash  $($packages[0].Name)" | Set-Content -LiteralPath $checksumFile -Encoding ascii

    if ($targetTags -notcontains $version) {
        Invoke-Checked -Command "jj" -Arguments @("tag", "set", $version, "-r", "fork")
        Invoke-Checked -Command "jj" -Arguments @(
            "git", "push", "--remote", "origin", "--tag", $version
        )
    }

    $notesFile = Join-Path $buildDirectory "release-notes.md"
    @"
Automated downstream fork build from commit $forkCommit.

This is an unofficial, unsigned KeePassXC Windows x64 portable build.
"@ | Set-Content -LiteralPath $notesFile -Encoding ascii
    Invoke-Checked -Command "gh" -Arguments @(
        "release", "create", $version,
        $packages[0].FullName,
        $checksumFile,
        "--repo", $repository,
        "--verify-tag",
        "--title", "KeePassXC $version",
        "--notes-file", $notesFile,
        "--latest"
    )
    Write-Host "Published https://github.com/$repository/releases/tag/$version"
} finally {
    Pop-Location
}
