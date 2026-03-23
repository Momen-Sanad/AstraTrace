param(
    [string]$Config = "Release",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
)

$ErrorActionPreference = "Stop"

$RootDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $RootDir "build"
$TargetName = "astratrace_app"

$KnownConfigs = @("Debug", "Release", "RelWithDebInfo", "MinSizeRel")
if ($KnownConfigs -notcontains $Config) {
    # If first arg is not a known CMake config, treat it as the first app argument.
    $AppArgs = @($Config) + $AppArgs
    $Config = "Release"
}

function Find-ExePath {
    $Candidates = @(
        (Join-Path $BuildDir "bin\$TargetName.exe"),
        (Join-Path $BuildDir "$TargetName.exe"),
        (Join-Path $BuildDir "bin\$Config\$TargetName.exe"),
        (Join-Path $BuildDir "$Config\$TargetName.exe")
    )

    return $Candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

function Get-ConfiguredBuildType {
    $CachePath = Join-Path $BuildDir "CMakeCache.txt"
    if (-not (Test-Path $CachePath)) {
        return ""
    }
    $Line = Select-String -Path $CachePath -Pattern '^CMAKE_BUILD_TYPE:STRING=' | Select-Object -First 1
    if (-not $Line) {
        return ""
    }
    return ($Line.Line -replace '^CMAKE_BUILD_TYPE:STRING=', '').Trim()
}

function Find-DefaultScene {
    $Candidates = @(
        (Join-Path $RootDir "assets\scene.gltf"),
        (Join-Path $RootDir "assets\scene.glb")
    )

    foreach ($Candidate in $Candidates) {
        if ($Candidate -and (Test-Path $Candidate)) {
            return $Candidate
        }
    }

    $AssetDir = Join-Path $RootDir "assets"
    if (Test-Path $AssetDir) {
        $AnyAssetScene = Get-ChildItem -Path $AssetDir -Recurse -File -Include *.gltf, *.glb -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
        if ($AnyAssetScene) {
            return $AnyAssetScene
        }
    }

    $BundledScene = Join-Path $RootDir "vendor\tinygltf\models\Cube\Cube.gltf"
    if (Test-Path $BundledScene) {
        return $BundledScene
    }

    return $null
}

$ConfiguredBuildType = Get-ConfiguredBuildType
$BuildTypeMismatch = $false
if ($ConfiguredBuildType -and ($ConfiguredBuildType.ToLowerInvariant() -ne $Config.ToLowerInvariant())) {
    Write-Host "Build type mismatch (configured: $ConfiguredBuildType, requested: $Config). Reconfiguring..."
    $BuildTypeMismatch = $true
}

$ExePath = $null
if (-not $BuildTypeMismatch) {
    $ExePath = Find-ExePath
}

if (-not $ExePath) {
    Write-Host "No existing executable found. Rebuilding from scratch..."
    if (Test-Path $BuildDir) {
        if (-not $BuildTypeMismatch) {
            Remove-Item -Path $BuildDir -Recurse -Force
        }
    }

    cmake -S $RootDir -B $BuildDir -DCMAKE_BUILD_TYPE=$Config
    cmake --build $BuildDir --config $Config --target $TargetName
    $ExePath = Find-ExePath
}

if (-not $ExePath) {
    throw "Could not find $TargetName.exe after fresh build."
}

if (-not $AppArgs -or $AppArgs.Count -eq 0) {
    $DefaultScene = Find-DefaultScene
    if (-not $DefaultScene) {
        throw "Usage: .\run.ps1 [-Config Release] <scene.gltf|scene.glb> [extra args]"
    }
    Write-Host "No scene argument provided. Using $DefaultScene"
    $AppArgs = @($DefaultScene)
}

# Normalize first app argument (scene path) so it still resolves after changing working directory.
if ($AppArgs.Count -gt 0) {
    $SceneArg = $AppArgs[0]
    if ($SceneArg) {
        if ([System.IO.Path]::IsPathRooted($SceneArg)) {
            if (Test-Path $SceneArg) {
                $AppArgs[0] = (Resolve-Path $SceneArg).Path
            }
        }
        else {
            $RootRelative = Join-Path $RootDir $SceneArg
            if (Test-Path $RootRelative) {
                $AppArgs[0] = (Resolve-Path $RootRelative).Path
            }
            elseif (Test-Path $SceneArg) {
                $AppArgs[0] = (Resolve-Path $SceneArg).Path
            }
        }
    }
}

# Keep a working directory where ../../assets resolves to the project assets path.
$RunDir = Join-Path $BuildDir $Config
if (-not (Test-Path $RunDir)) {
    New-Item -ItemType Directory -Path $RunDir | Out-Null
}

Write-Host "Running $ExePath"
Push-Location $RunDir
try {
    & $ExePath @AppArgs
}
finally {
    Pop-Location
}
