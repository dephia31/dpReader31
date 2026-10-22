<#
.SYNOPSIS
    Moku 的按板构建入口；共用 Python 构建器生成独立源码视图和 USB 包。
    Moku's board entry; use the common Python builder for isolated source views and USB bundles.
.EXAMPLE
    .\tools\build_board.ps1 -Board metalio_s31
#>
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("pico", "metalio47", "metalio_s31", "all")]
    [string]$Board,
    [string]$Output = "build/boards",
    [string]$Python = "python"
)
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $env:IDF_PATH) { throw "先载入对应板型的 ESP-IDF / Load this board's ESP-IDF first." }
Push-Location $repo
try {
    $pythonPath = (Get-Command $Python -ErrorAction Stop).Source
    & $pythonPath tools/build_firmware.py --board $Board --output $Output --python $pythonPath
    if ($LASTEXITCODE -ne 0) { throw "Board build failed ($LASTEXITCODE)" }
} finally { Pop-Location }
