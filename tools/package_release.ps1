param(
    [Parameter(Mandatory = $true)]
    [string]$BuildReleaseDirectory,
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,
    [string]$Version = "1.4.5"
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
Copy-Item -LiteralPath (Join-Path $repoRoot "third_party\openvino-2026.2.1") `
    -Destination (Join-Path $binaryRoot "third_party\openvino-2026.2.1") -Recurse
New-Item -ItemType Directory -Path (Join-Path $binaryRoot "tools\onnx_upscaler") -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "tools\onnx_upscaler\README.md") `
    -Destination (Join-Path $binaryRoot "tools\onnx_upscaler\README.md")

Copy-Item -LiteralPath (Join-Path $buildRoot "TenRiff.exe") -Destination $binaryRoot
$runtimeFiles = @(
    "openvino.dll",
    "openvino_intel_cpu_plugin.dll",
    "openvino_intel_gpu_plugin.dll",
    "openvino_intel_npu_plugin.dll",
    "openvino_intel_npu_compiler_loader.dll",
    "openvino_intel_npu_compiler.dll",
    "openvino_onnx_frontend.dll",
    "cache.json",
    "tbb12.dll",
    "tbbbind_2_5.dll",
    "tbbmalloc.dll",
    "tbbmalloc_proxy.dll"
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
