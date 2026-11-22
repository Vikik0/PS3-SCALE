[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$GscToolPath,

    [Parameter(Mandatory = $true)]
    [string]$InputDirectory,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"

$tool = (Resolve-Path -LiteralPath $GscToolPath).Path
$inputRoot = (Resolve-Path -LiteralPath $InputDirectory).Path.TrimEnd('\', '/')
if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
    throw "GSC Tool executable not found: $tool"
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$outputRoot = (Resolve-Path -LiteralPath $OutputDirectory).Path

$files = Get-ChildItem -LiteralPath $inputRoot -Recurse -File |
    Where-Object { $_.Extension.ToLowerInvariant() -in @(".gscbin", ".cscbin") }

if ($files.Count -eq 0) {
    Write-Error "No compiled .gscbin/.cscbin assets found under $inputRoot"
    exit 2
}

$failed = 0
foreach ($file in $files) {
    $relative = $file.FullName.Substring($inputRoot.Length).TrimStart([char[]]@('\', '/'))
    $target = Join-Path $outputRoot $relative
    $targetDirectory = Split-Path -Parent $target
    New-Item -ItemType Directory -Force -Path $targetDirectory | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $target -Force

    Write-Host "Decompiling $relative"
    & $tool -m decomp -g iw6 -s ps3 $target
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "GSC Tool failed with exit code $LASTEXITCODE for $relative"
        $failed++
    }
}

Write-Host "Decompilation finished. Files: $($files.Count), failures: $failed"
if ($failed -gt 0) {
    exit 1
}
exit 0
