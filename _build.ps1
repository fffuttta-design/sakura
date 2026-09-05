# 開発中の「速いビルド」（配信はしない・テストも流さない）
#
# _release.ps1 との違い:
#   _release.ps1 … 版番号を作り直す（version.h を消す）＋テスト1215件＋配布 = 4〜5分
#   _build.ps1   … 本体(sakura.exe)だけを作る = 30〜60秒
#
# 使い方:
#   & "C:\dev\CompanyOps\Application\Utility\sakura\_build.ps1"            # Debug
#   & "C:\dev\CompanyOps\Application\Utility\sakura\_build.ps1" -Release   # Release
#   & "C:\dev\CompanyOps\Application\Utility\sakura\_build.ps1" -Tests     # tests1.exe も作る
param(
    [switch]$Release,
    [switch]$Tests
)
$ErrorActionPreference = 'Stop'
$Root = 'C:\dev\CompanyOps\Application\Utility\sakura'
$Cfg  = if ($Release) { 'Release' } else { 'Debug' }

# 🔥 githash.h を作り直さない。作り直すと、それを読む物が軒並み再コンパイルになる
$env:SKIP_CREATE_GITHASH = '1'
# 🔥 この端末は NoDefaultCurrentDirectoryInExePath が効いていて、
#    カレントの .bat を名前だけで呼べない（CLAUDE.local.md 参照）
$env:NoDefaultCurrentDirectoryInExePath = ''

# 起動中の開発版があるとリンクで失敗する（LNK1104）ので先に落とす
Get-Process sakura -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -like "*x64\$Cfg*" } | Stop-Process -Force -ErrorAction SilentlyContinue

$msbuild = & "$Root\tools\find-tools.bat" 2>$null
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath  = & $vswhere -latest -requires Microsoft.Component.MSBuild -property installationPath
$msbuild = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'

$proj = if ($Tests) { Join-Path $Root 'sakura.sln' } else { Join-Path $Root 'sakura_core\sakura.vcxproj' }
$sw = [Diagnostics.Stopwatch]::StartNew()
& $msbuild $proj /nologo /m /v:minimal /p:Platform=x64 /p:Configuration=$Cfg /t:Build
$ok = ($LASTEXITCODE -eq 0)
$sw.Stop()

if ($ok) {
    $exe = Join-Path $Root "x64\$Cfg\sakura.exe"
    Write-Host ("ビルド成功 {0}  {1:N1}秒  {2}" -f $Cfg, $sw.Elapsed.TotalSeconds, (Get-Item $exe).VersionInfo.ProductVersion) -ForegroundColor Green
} else {
    Write-Host ("ビルド失敗 {0}  {1:N1}秒" -f $Cfg, $sw.Elapsed.TotalSeconds) -ForegroundColor Red
    exit 1
}
