param(
    [Parameter(Mandatory = $true)]
    [string]$BuildReleaseDirectory,
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,
    [string]$Version = "1.4.5.3"
)

$ErrorActionPreference = "Stop"
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildRoot = [System.IO.Path]::GetFullPath($BuildReleaseDirectory)
$assetsRoot = [System.IO.Path]::GetFullPath($OutputDirectory)
$binaryName = "TenRiff-$Version"
$sourceName = "TenRiff-$Version-source"
$binaryRoot = Join-Path $assetsRoot $binaryName
$sourceRoot = Join-Path $assetsRoot $sourceName
$binaryZip = Join-Path $assetsRoot "$binaryName.zip"
$sourceZip = Join-Path $assetsRoot "$sourceName.zip"
$sumsPath = Join-Path $assetsRoot "TenRiff-$Version-SHA256SUMS.txt"

if (Test-Path -LiteralPath $assetsRoot) {
    throw "Output directory already exists: $assetsRoot"
}
if (-not (Test-Path -LiteralPath (Join-Path $buildRoot "TenRiff.exe"))) {
    throw "TenRiff.exe was not found in: $buildRoot"
}
if (Get-ChildItem -LiteralPath $buildRoot -Filter "bms_key_converter*.exe" -ErrorAction SilentlyContinue) {
    throw "Standalone BMS key-converter binaries must not be packaged."
}

$buildTree = Split-Path -Parent $buildRoot
$cmakeCache = Join-Path $buildTree "CMakeCache.txt"
if (-not (Test-Path -LiteralPath $cmakeCache -PathType Leaf)) {
    throw "CMakeCache.txt was not found for the release build: $cmakeCache"
}
$ctestCacheEntry = Get-Content -LiteralPath $cmakeCache |
    Where-Object { $_ -like "CMAKE_CTEST_COMMAND:INTERNAL=*" } |
    Select-Object -First 1
if (-not $ctestCacheEntry) {
    throw "CMAKE_CTEST_COMMAND was not found in: $cmakeCache"
}
$ctestCommand = ($ctestCacheEntry -split "=", 2)[1]
if (-not (Test-Path -LiteralPath $ctestCommand -PathType Leaf)) {
    throw "CTest executable was not found: $ctestCommand"
}

$testInventoryJson = & $ctestCommand --test-dir $buildTree -C Release --show-only=json-v1
if ($LASTEXITCODE -ne 0) {
    throw "Could not enumerate the release tests."
}
$testInventory = ($testInventoryJson -join "`n") | ConvertFrom-Json
$releaseTestCount = @($testInventory.tests).Count
if ($releaseTestCount -eq 0) {
    throw "The release build has no registered CTest tests. Configure with TENRIFF_ENABLE_TESTS=ON."
}

Write-Host "Running $releaseTestCount release tests before packaging..."
& $ctestCommand --test-dir $buildTree -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "Release tests failed; packaging was stopped."
}

New-Item -ItemType Directory -Path $binaryRoot, $sourceRoot -Force | Out-Null

$topFiles = @(
    "CHANGELOG.md",
    "launch_win.bat",
    "LICENSE",
    "README.md",
    "README.en.md",
    "README.ja.md",
    "README.zh-CN.md",
    "THIRD_PARTY_NOTICES.md"
)
foreach ($relative in $topFiles) {
    Copy-Item -LiteralPath (Join-Path $repoRoot $relative) -Destination $binaryRoot
}

foreach ($directory in @("Mainmusic", "config", "docs", "examples", "models")) {
    Copy-Item -LiteralPath (Join-Path $repoRoot $directory) -Destination $binaryRoot -Recurse
}
$ncnnNoticeDirectory = Join-Path $binaryRoot "third_party\ncnn-20260526"
New-Item -ItemType Directory -Path $ncnnNoticeDirectory -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "third_party\ncnn-20260526\LICENSE.txt") `
    -Destination (Join-Path $ncnnNoticeDirectory "LICENSE.txt")
New-Item -ItemType Directory -Path (Join-Path $binaryRoot "tools\onnx_upscaler") -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "tools\onnx_upscaler\README.md") `
    -Destination (Join-Path $binaryRoot "tools\onnx_upscaler\README.md")

Copy-Item -LiteralPath (Join-Path $buildRoot "TenRiff.exe") -Destination $binaryRoot
$runtimeFiles = @(
    "ncnn.dll"
)
foreach ($runtime in $runtimeFiles) {
    $runtimePath = Join-Path $buildRoot $runtime
    if (-not (Test-Path -LiteralPath $runtimePath)) {
        throw "Required release runtime is missing: $runtimePath"
    }
    Copy-Item -LiteralPath $runtimePath -Destination $binaryRoot
}

$sourceFiles = & git -C $repoRoot ls-files --cached --others --exclude-standard
if ($LASTEXITCODE -ne 0 -or -not $sourceFiles) {
    throw "Could not enumerate the public source files."
}
foreach ($relative in $sourceFiles) {
    $sourcePath = Join-Path $repoRoot $relative
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Enumerated source file is missing: $relative"
    }
    $destination = Join-Path $sourceRoot $relative
    $destinationParent = Split-Path -Parent $destination
    New-Item -ItemType Directory -Path $destinationParent -Force | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $destination
}

Compress-Archive -LiteralPath $binaryRoot -DestinationPath $binaryZip -CompressionLevel Optimal
Compress-Archive -LiteralPath $sourceRoot -DestinationPath $sourceZip -CompressionLevel Optimal

$sumLines = foreach ($archive in @($binaryZip, $sourceZip)) {
    $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    "$hash  $(Split-Path -Leaf $archive)"
}
Set-Content -LiteralPath $sumsPath -Value $sumLines -Encoding ascii

Get-Item -LiteralPath $binaryZip, $sourceZip, $sumsPath |
    Select-Object FullName, Length, LastWriteTime
