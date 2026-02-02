$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Need-Cmd([string]$name) {
  if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
    throw "Missing required command: $name"
  }
}

function Ensure-Go {
  if (Get-Command go -ErrorAction SilentlyContinue) {
    return
  }

  $goVersion = if ($env:GO_VERSION) { $env:GO_VERSION } else { "1.22.5" }
  $goDir = Join-Path $root (Join-Path "out\tools\go" $goVersion)
  $goBin = Join-Path $goDir "go\bin\go.exe"

  if (Test-Path $goBin) {
    $env:PATH = (Join-Path $goDir "go\bin") + ";" + $env:PATH
    return
  }

  $arch = $env:PROCESSOR_ARCHITECTURE
  $goArch = switch ($arch) {
    "AMD64" { "amd64" }
    "ARM64" { "arm64" }
    default { throw "Unsupported arch for Go bootstrap: $arch" }
  }

  $zip = "go$goVersion.windows-$goArch.zip"
  $url = "https://go.dev/dl/$zip"
  $tmp = Join-Path $goDir $zip

  New-Item -ItemType Directory -Force -Path $goDir | Out-Null
  Write-Host "Bootstrapping Go $goVersion (windows/$goArch)..."
  Invoke-WebRequest -Uri $url -OutFile $tmp
  Expand-Archive -Path $tmp -DestinationPath $goDir -Force
  Remove-Item $tmp -Force

  $env:PATH = (Join-Path $goDir "go\bin") + ";" + $env:PATH
}

function Ensure-EngineBuild {
  Need-Cmd cmake

  . (Join-Path $root "scripts\vsdev.ps1")

  $preset = if ($env:ZIREAEL_PRESET) { $env:ZIREAEL_PRESET } else { "windows-clangcl-release" }
  $buildDir = Join-Path $root (Join-Path "out\build" $preset)
  $cache = Join-Path $buildDir "CMakeCache.txt"

  $needsConfigure = $true
  if (Test-Path $cache) {
    $line = Select-String -Path $cache -Pattern "^ZIREAEL_BUILD_SHARED:BOOL=" -ErrorAction SilentlyContinue
    if ($line -and $line.Line -match "ZIREAEL_BUILD_SHARED:BOOL=ON") {
      $needsConfigure = $false
    }
  }

  if ($needsConfigure) {
    Write-Host "Configuring Zireael ($preset, shared ON)..."
    cmake --preset $preset -DZIREAEL_BUILD_SHARED=ON
  }

  Write-Host "Building Zireael ($preset)..."
  cmake --build --preset $preset

  $dll = Join-Path $buildDir "zireael.dll"
  if (-not (Test-Path $dll)) {
    throw "Expected DLL not found: $dll"
  }
  $env:ZR_DLL_PATH = $dll
}

Ensure-Go
Ensure-EngineBuild

Push-Location (Join-Path $root "poc\go-codex-tui")
try {
  & go run . @args
  exit $LASTEXITCODE
}
finally {
  Pop-Location
}
