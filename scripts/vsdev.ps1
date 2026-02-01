$ErrorActionPreference = 'Stop'

function Get-VsDevCmdPath {
  $candidates = @(
    'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat',
    'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
  )

  foreach ($p in $candidates) {
    if (Test-Path $p) { return $p }
  }

  return $null
}

$vsDevCmd = Get-VsDevCmdPath
if (-not $vsDevCmd) {
  throw "VsDevCmd.bat not found. Install Visual Studio 2022 with 'Desktop development with C++' (MSVC + Windows SDK)."
}

# Import the MSVC/Windows SDK environment into the current PowerShell session.
# This is required for Ninja+clang-cl presets to find link libraries (kernel32.lib, msvcrtd.lib, etc.).
$cmd = "`"$vsDevCmd`" -no_logo -arch=x64 -host_arch=x64 && set"
$lines = & cmd.exe /s /c $cmd
if ($LASTEXITCODE -ne 0) {
  throw "VsDevCmd.bat failed with exit code $LASTEXITCODE"
}

foreach ($line in $lines) {
  $i = $line.IndexOf('=')
  if ($i -le 0) { continue }
  $k = $line.Substring(0, $i)
  $v = $line.Substring($i + 1)
  if ($k -eq '?') { continue }
  Set-Item -Path ("Env:" + $k) -Value $v
}

# Some VS installs fail to populate WindowsSdkDir/WindowsSDKVersion even when the SDK is present on disk.
# If that happens, synthesize SDK env + LIB/INCLUDE from the Windows Kits layout.
$sdkRoot = 'C:\Program Files (x86)\Windows Kits\10'
$sdkIncludeRoot = Join-Path $sdkRoot 'Include'
$sdkLibRoot = Join-Path $sdkRoot 'Lib'

$sdkVersion = $null
if (Test-Path $sdkIncludeRoot) {
  $sdkVersion = (Get-ChildItem -Directory $sdkIncludeRoot | Sort-Object Name -Descending | Select-Object -First 1).Name
}

if ($sdkVersion -and [string]::IsNullOrWhiteSpace($env:WindowsSdkDir)) {
  $env:WindowsSdkDir = ($sdkRoot + '\')
  $env:WindowsSDKVersion = ($sdkVersion + '\')
  $env:UniversalCRTSdkDir = ($sdkRoot + '\')
}

if ($sdkVersion) {
  $inc = @(
    (Join-Path $sdkIncludeRoot (Join-Path $sdkVersion 'ucrt')),
    (Join-Path $sdkIncludeRoot (Join-Path $sdkVersion 'shared')),
    (Join-Path $sdkIncludeRoot (Join-Path $sdkVersion 'um')),
    (Join-Path $sdkIncludeRoot (Join-Path $sdkVersion 'winrt')),
    (Join-Path $sdkIncludeRoot (Join-Path $sdkVersion 'cppwinrt'))
  ) | Where-Object { Test-Path $_ }

  $lib = @(
    (Join-Path $sdkLibRoot (Join-Path $sdkVersion 'ucrt\x64')),
    (Join-Path $sdkLibRoot (Join-Path $sdkVersion 'um\x64'))
  ) | Where-Object { Test-Path $_ }

  foreach ($p in $inc) {
    $escaped = [Regex]::Escape($p)
    if ($env:INCLUDE -notmatch "(^|;)${escaped}(;|$)") {
      $env:INCLUDE = $env:INCLUDE + ';' + $p
    }
  }

  foreach ($p in $lib) {
    $escaped = [Regex]::Escape($p)
    if ($env:LIB -notmatch "(^|;)${escaped}(;|$)") {
      $env:LIB = $env:LIB + ';' + $p
    }
  }
}

# Make sure common tools are discoverable in this session (helpful for shells that don't reload user PATH yet).
$extra = @(
  'C:\Program Files\CMake\bin',
  'C:\Program Files\LLVM\bin'
)
foreach ($p in $extra) {
  if (Test-Path $p) {
    $escaped = [Regex]::Escape($p)
    if ($env:Path -notmatch "(^|;)${escaped}(;|$)") {
      $env:Path = $p + ';' + $env:Path
    }
  }
}

Write-Host "VS dev environment loaded ($vsDevCmd)."
