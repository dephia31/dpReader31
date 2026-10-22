<#
.SYNOPSIS
    按板构建/烧录/监视 Kiikoread：-Board pico 或 metalio_s31。

.DESCRIPTION
    一套代码两块板：本脚本只是 idf.py 的板级包装，负责选对 build 目录与 -DKIIKO_BOARD，
    避免两块板共用 build 目录或 sdkconfig 而互相改写配置。

    需要先载入 ESP-IDF（export.ps1）。默认 build 目录为仓库根的 build-<板名>。

.EXAMPLE
    .\tools\build_board.ps1 -Board pico build
    .\tools\build_board.ps1 -Board metalio_s31 -Action flash -Port COM7
    .\tools\build_board.ps1 -Board metalio_s31 menuconfig
#>
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("pico", "metalio_s31")]
    [string]$Board,

    [Parameter(Position = 0)]
    [ValidateSet("build", "flash", "monitor", "menuconfig", "size", "clean", "fullclean", "reconfigure", "partition-table")]
    [string]$Action = "build",

    [string]$Port = "",
    [string]$BuildDir = "",
    [switch]$Flash
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

if (-not $BuildDir) {
    $BuildDir = Join-Path $repo "build-$Board"
}

if (-not $env:IDF_PATH) {
    throw "ESP-IDF 未载入：先执行 `$env:IDF_PATH\export.ps1`（参见 README）。"
}
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    throw "找不到 idf.py：ESP-IDF 环境没载入完整。"
}

$boardFile = Join-Path $repo "boards/$Board/board.cmake"
if (-not (Test-Path $boardFile)) {
    throw "未知板级 '$Board'：boards/ 下没有 $boardFile。"
}

# 板级 CMake 变量必须每块板都显式给，否则同一个 CMake 缓存会沿用上一块板的目标与 sdkconfig。
# The board variables are passed explicitly every time so one CMake cache cannot inherit the other
# board's target or sdkconfig.
$target = switch ($Board) {
    "pico" { "esp32s3" }
    "metalio_s31" { "esp32s31" }
}

$arguments = @(
    "-B", $BuildDir,
    "-D", "KIIKO_BOARD=$Board",
    "-D", "IDF_TARGET=$target"
)

Write-Host "board=$Board target=$target build=$BuildDir action=$Action"

Push-Location $repo
try {
    if ($Action -eq "flash") {
        if ($Port) { & idf.py @arguments -p $Port flash }
        else { & idf.py @arguments flash }
    }
    elseif ($Action -eq "monitor") {
        if ($Port) { & idf.py @arguments -p $Port monitor }
        else { & idf.py @arguments monitor }
    }
    elseif ($Action -eq "menuconfig") {
        & idf.py @arguments menuconfig
    }
    else {
        & idf.py @arguments $Action
    }
    if ($LASTEXITCODE -ne 0) { throw "idf.py $Action 失败（exit $LASTEXITCODE）" }
}
finally {
    Pop-Location
}

$bin = Get-ChildItem -Path (Join-Path $BuildDir "*.bin") -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -notmatch "bootloader|partition|ota_data|flashed" } |
    Select-Object -First 1
if ($bin) {
    Write-Host ("artifact: {0} ({1} bytes)" -f $bin.FullName, $bin.Length)
}
