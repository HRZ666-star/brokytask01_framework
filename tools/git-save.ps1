<#
.SYNOPSIS
    一键提交并推送当前仓库的改动。

.DESCRIPTION
    在仓库任意位置执行都可以。脚本会自动定位到仓库根目录，
    显示改动、执行 git add -A、提交、推送。
    首次推送新分支时会自动设置 upstream。

.PARAMETER Message
    提交信息，建议使用 feat/fix/docs/test/chore 前缀。

.PARAMETER NoPush
    只提交，不推送。

.EXAMPLE
    .\tools\git-save.ps1 "feat(bsp): add uart dma"

.EXAMPLE
    .\tools\git-save.ps1 "docs: update porting steps" -NoPush
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Message,

    [switch]$NoPush
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (-not (Test-Path (Join-Path $repoRoot '.git'))) {
    throw "不是 Git 仓库：$repoRoot"
}

$status = git status --porcelain
if (-not $status) {
    Write-Host "没有需要提交的改动。" -ForegroundColor Yellow
    exit 0
}

Write-Host "===== 改动 =====" -ForegroundColor Cyan
git status --short
git diff --stat

Write-Host "`n===== 提交 =====" -ForegroundColor Cyan
git add -A
git commit -m $Message
if ($LASTEXITCODE -ne 0) {
    throw "git commit 失败，请检查上面的输出。"
}

$hash = git rev-parse --short HEAD
Write-Host "已提交 $hash  $Message" -ForegroundColor Green

if ($NoPush) {
    Write-Host "已跳过 push（-NoPush）。" -ForegroundColor Yellow
    exit 0
}

$branch = git rev-parse --abbrev-ref HEAD
$upstream = git rev-parse --abbrev-ref --symbolic-full-name '@{u}' 2>$null

if (-not $upstream) {
    git push -u origin $branch
} else {
    git push
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "`npush 失败。如果远端有新提交，先执行：" -ForegroundColor Red
    Write-Host "  git pull --rebase" -ForegroundColor Yellow
    Write-Host "  git push" -ForegroundColor Yellow
    exit 1
}

Write-Host "已推送到 origin/$branch。" -ForegroundColor Green
