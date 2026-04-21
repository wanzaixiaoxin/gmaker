# gmaker 服务停止脚本
# 用法: .\scripts\stop-services.ps1 [all|registry|biz|dbproxy|gateway|logstats]

param(
    [Parameter()]
    [ValidateSet("all", "registry", "biz", "dbproxy", "gateway", "logstats")]
    [string]$Target = "all"
)

$exeMap = @{
    "registry" = "registry-go"
    "biz"      = "biz-go"
    "dbproxy"  = "dbproxy-go"
    "gateway"  = "gateway-cpp"
    "logstats" = "logstats-go"
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "  gmaker Service Stopper" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

if ($Target -eq "all") {
    foreach ($name in $exeMap.Values) {
        $proc = Get-Process -Name $name -ErrorAction SilentlyContinue
        if ($proc) {
            Stop-Process -Name $name -Force
            Write-Host "  Stopped $name" -ForegroundColor Green
        } else {
            Write-Host "  $name not running" -ForegroundColor DarkGray
        }
    }
} else {
    $name = $exeMap[$Target]
    $proc = Get-Process -Name $name -ErrorAction SilentlyContinue
    if ($proc) {
        Stop-Process -Name $name -Force
        Write-Host "  Stopped $name" -ForegroundColor Green
    } else {
        Write-Host "  $name not running" -ForegroundColor DarkGray
    }
}

Write-Host ""
Write-Host "Done." -ForegroundColor Green
