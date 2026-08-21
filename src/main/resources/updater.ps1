# SakuraEditorPlus 更新実行スクリプト
#
# アプリ本体（sakura.exe）から呼ばれる。直接実行するものではない。
#
#   1. 「更新しています」の窓を出す
#   2. アプリが全部終わるのを待つ（動いている exe は上書きできない）
#   3. ファイルをコピーする（進捗バーを進める）
#   4. アプリを起動し直して、窓を閉じる
#
# 自分自身も更新対象なので、呼び出し側が %TEMP% にコピーしてから起動する。
# （導入先で直接動かすと、自分を上書きしようとして失敗する）

param(
    [Parameter(Mandatory=$true)][string]$SrcDir,   # 配布元
    [Parameter(Mandatory=$true)][string]$DstDir    # 導入先
)

$ErrorActionPreference = 'Stop'

# --- 高DPI宣言（窓を作る前に必ずやる）---------------------------------------
# 宣言しないと Windows が勝手に拡大表示して、文字がにじんだ窓になる。
try {
    Add-Type -Namespace SkrDpi -Name Api -MemberDefinition @'
[DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
'@
    [SkrDpi.Api]::SetProcessDPIAware() | Out-Null
} catch {}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# 宣言しただけでは座標は 96dpi のままなので、自分で倍率をかける
$g0    = [System.Drawing.Graphics]::FromHwnd([IntPtr]::Zero)
$scale = $g0.DpiX / 96.0
$g0.Dispose()
function Px([int]$n) { return [int][Math]::Round($n * $scale) }

# --- 窓を組み立てる ---------------------------------------------------------
$form                 = New-Object System.Windows.Forms.Form
$form.Text            = 'SakuraEditorPlus の更新'
$form.ClientSize      = New-Object System.Drawing.Size((Px 410), (Px 112))
$form.StartPosition   = 'CenterScreen'
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox     = $false
$form.MinimizeBox     = $false
$form.ControlBox      = $false          # 途中で閉じられると中途半端になるので × を出さない
$form.TopMost         = $true

$icon = Join-Path $DstDir 'sakura.exe'
if (Test-Path $icon) {
    try { $form.Icon = [System.Drawing.Icon]::ExtractAssociatedIcon($icon) } catch {}
}

$labelMain            = New-Object System.Windows.Forms.Label
$labelMain.Text       = '更新しています...'
$labelMain.Location   = New-Object System.Drawing.Point((Px 20), (Px 18))
$labelMain.Size       = New-Object System.Drawing.Size((Px 370), (Px 24))
$labelMain.Font       = New-Object System.Drawing.Font($form.Font.FontFamily, 10)
$form.Controls.Add($labelMain)

$bar                  = New-Object System.Windows.Forms.ProgressBar
$bar.Location         = New-Object System.Drawing.Point((Px 20), (Px 50))
$bar.Size             = New-Object System.Drawing.Size((Px 370), (Px 20))
$bar.Style            = 'Marquee'      # 待っている間は流れる表示
$bar.MarqueeAnimationSpeed = 30
$form.Controls.Add($bar)

$labelSub             = New-Object System.Windows.Forms.Label
$labelSub.Text        = ''
$labelSub.Location    = New-Object System.Drawing.Point((Px 20), (Px 78))
$labelSub.Size        = New-Object System.Drawing.Size((Px 370), (Px 20))
$labelSub.ForeColor   = [System.Drawing.Color]::Gray
$form.Controls.Add($labelSub)

$form.Show()
[System.Windows.Forms.Application]::DoEvents()

function Set-Status([string]$main, [string]$sub) {
    if ($main) { $labelMain.Text = $main }
    $labelSub.Text = $sub
    [System.Windows.Forms.Application]::DoEvents()
}

try {
    # --- 1. アプリが終わるのを待つ ------------------------------------------
    Set-Status '更新しています...' 'アプリの終了を待っています'
    $exe = Join-Path $DstDir 'sakura.exe'
    $waited = 0
    while ($waited -lt 60) {
        $running = @(Get-Process -Name 'sakura' -ErrorAction SilentlyContinue |
                     Where-Object { $_.Path -like "$DstDir*" })
        if ($running.Count -eq 0) { break }
        Start-Sleep -Milliseconds 500
        $waited += 0.5
        [System.Windows.Forms.Application]::DoEvents()
    }
    # 念のため、exe が書き込めるようになるまでもう少し待つ
    for ($i = 0; $i -lt 20; $i++) {
        try {
            $fs = [System.IO.File]::Open($exe, 'Open', 'Write', 'None')
            $fs.Close()
            break
        } catch {
            Start-Sleep -Milliseconds 500
            [System.Windows.Forms.Application]::DoEvents()
        }
    }

    # --- 2. コピーする -------------------------------------------------------
    # 設定ファイルは持っていかない（引き継いだ設定が消えてしまう）
    $exclude = @('sakura.ini', 'update-check.txt')
    $files = @(Get-ChildItem $SrcDir -Recurse -File |
               Where-Object { $exclude -notcontains $_.Name -and $_.Name -notlike 'sakura.ini.*' })

    $bar.Style = 'Continuous'          # 件数が分かったので、ここから本物の進捗にする
    $bar.Minimum = 0
    $bar.Maximum = [Math]::Max(1, $files.Count)
    $bar.Value   = 0
    Set-Status '更新しています...' ('0 / {0}' -f $files.Count)

    $done = 0
    foreach ($f in $files) {
        $rel = $f.FullName.Substring($SrcDir.Length).TrimStart('\')
        $to  = Join-Path $DstDir $rel
        $dir = Split-Path $to -Parent
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }

        # 掴まれていることがあるので少し粘る
        for ($retry = 0; $retry -lt 20; $retry++) {
            try { Copy-Item $f.FullName $to -Force; break }
            catch { Start-Sleep -Milliseconds 500; [System.Windows.Forms.Application]::DoEvents() }
        }

        $done++
        $bar.Value = [Math]::Min($bar.Maximum, $done)
        Set-Status $null ('{0} / {1}' -f $done, $files.Count)
    }

    # --- 3. 起動し直す -------------------------------------------------------
    Set-Status '起動しています...' ''
    Start-Sleep -Milliseconds 400
    if (Test-Path $exe) { Start-Process $exe }
    Start-Sleep -Milliseconds 800
}
catch {
    Set-Status '更新に失敗しました' $_.Exception.Message
    $bar.Style = 'Continuous'
    [System.Windows.Forms.Application]::DoEvents()
    [System.Windows.Forms.MessageBox]::Show(
        ("更新に失敗しました。`n`n{0}`n`nお手数ですが、配布フォルダーの install-sakura.ps1 を実行してください。" -f $_.Exception.Message),
        'SakuraEditorPlus の更新',
        [System.Windows.Forms.MessageBoxButtons]::OK,
        [System.Windows.Forms.MessageBoxIcon]::Warning) | Out-Null
    # 失敗しても、元のアプリは起動しておく
    if (Test-Path $exe) { Start-Process $exe }
}
finally {
    $form.Close()
    $form.Dispose()
}
