# Generate protobuf code for Go and C++

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root\..

$protoDir = "spec/proto"
$genGoDir = "gen/go"
$genCppDir = "gen/cpp"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Protobuf Code Generation" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "[1/5] Checking tools..." -ForegroundColor Yellow
$protocVersion = (& protoc --version 2>$null)
if ($LASTEXITCODE -ne 0) {
    Write-Error "ERROR: protoc not found in PATH"
    exit 1
}
Write-Host "  $protocVersion"

$goPlugin = Get-Command protoc-gen-go -ErrorAction SilentlyContinue
$grpcPlugin = Get-Command protoc-gen-go-grpc -ErrorAction SilentlyContinue
if ($goPlugin) { Write-Host "  protoc-gen-go      : $($goPlugin.Source)" }
if ($grpcPlugin) { Write-Host "  protoc-gen-go-grpc : $($grpcPlugin.Source)" }

Write-Host ""
Write-Host "[2/5] Configuration:" -ForegroundColor Yellow
Write-Host "  Proto dir : $protoDir"
Write-Host "  Go output : $genGoDir"
Write-Host "  C++ output: $genCppDir"
Write-Host "  Module    : github.com/gmaker/luffa"

Write-Host ""
Write-Host "[3/5] Proto files to process:" -ForegroundColor Yellow
$protoFiles = Get-ChildItem "$protoDir/*.proto"
foreach ($f in $protoFiles) {
    Write-Host "  - $($f.Name)"
}

Write-Host ""
Write-Host "[4/5] Running protoc..." -ForegroundColor Yellow

New-Item -ItemType Directory -Force -Path $genGoDir | Out-Null
New-Item -ItemType Directory -Force -Path $genCppDir | Out-Null

& protoc `
    --proto_path=$protoDir `
    --go_out=. --go_opt=module=github.com/gmaker/luffa `
    --go-grpc_out=. --go-grpc_opt=module=github.com/gmaker/luffa `
    --cpp_out=$genCppDir `
    ($protoFiles | ForEach-Object { $_.Name })

# 额外生成 protocol.proto（命令 ID 定义）
Write-Host "  Generating protocol.proto..."
& protoc `
    --proto_path=proto `
    --go_out=. --go_opt=module=github.com/gmaker/luffa `
    --cpp_out=$genCppDir `
    protocol.proto
if ($LASTEXITCODE -ne 0) {
    Write-Error "ERROR: protocol.proto generation failed!"
    exit $LASTEXITCODE
}

if ($LASTEXITCODE -ne 0) {
    Write-Error "ERROR: Protobuf generation failed! (exit code $LASTEXITCODE)"
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "[5/5] Generated files:" -ForegroundColor Yellow
Write-Host "  Go files:"
Get-ChildItem "$genGoDir/*/*.pb.go" -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host "    $($_.FullName)"
}
Write-Host "  C++ files:"
Get-ChildItem "$genCppDir/*.pb.cc" -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host "    $($_.FullName)"
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host " Protobuf generation done." -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
