# SakuraEditorPlus 更新実行スクリプト
#
# アプリ本体（sakura.exe）から呼ばれる。直接実行するものではない。
#
#   1. 「更新しています」の窓を出す
#   2. アプリが全部終わるのを待つ（動いている exe は上書きできない）
#   3. ファイルをコピーする（進捗バーを進める）
#   4. アプリを静かに起動し直して、窓を閉じる（前面には出さない）
#
# 自分自身も更新対象なので、呼び出し側が %TEMP% にコピーしてから起動する。
# （導入先で直接動かすと、自分を上書きしようとして失敗する）

param(
    [Parameter(Mandatory=$true)][string]$SrcDir,   # 配布元
    [Parameter(Mandatory=$true)][string]$DstDir    # 導入先
)

$ErrorActionPreference = 'Stop'

# --- Windows API（高DPI宣言と、更新後にアプリを前面へ出すのに使う）-----------
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class SkrWin
{
    public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc proc, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr hWnd, StringBuilder buf, int cch);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern void SwitchToThisWindow(IntPtr hWnd, bool altTab);
    [DllImport("user32.dll")] public static extern bool AllowSetForegroundWindow(int pid);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint idAttach, uint idAttachTo, bool fAttach);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
    [DllImport("user32.dll", EntryPoint = "SystemParametersInfoW", CharSet = CharSet.Unicode)]
    public static extern bool SpiGet(uint uiAction, uint uiParam, ref uint pvParam, uint fWinIni);
    [DllImport("user32.dll", EntryPoint = "SystemParametersInfoW", CharSet = CharSet.Unicode)]
    public static extern bool SpiSet(uint uiAction, uint uiParam, IntPtr pvParam, uint fWinIni);

    // 🔥 SetForegroundWindow は OS のフォーカス保護でほぼ断られる。
    //    「前面を奪う待ち時間」を一時的に 0 にし、前面スレッドに入力をつないでから呼ぶと通る。
    public static void ForceForeground(IntPtr hWnd)
    {
        if (hWnd == IntPtr.Zero) { return; }
        ShowWindow(hWnd, 9);   // SW_RESTORE（最小化されていても戻す）

        uint oldTimeout = 0;
        SpiGet(0x2000, 0, ref oldTimeout, 0);              // SPI_GETFOREGROUNDLOCKTIMEOUT
        SpiSet(0x2001, 0, IntPtr.Zero, 0x02);              // SPI_SETFOREGROUNDLOCKTIMEOUT = 0

        IntPtr fg = GetForegroundWindow();
        uint fgPid = 0;
        uint fgThread = GetWindowThreadProcessId(fg, out fgPid);
        uint myThread = GetCurrentThreadId();
        bool attached = false;
        if (fgThread != 0 && fgThread != myThread) { attached = AttachThreadInput(myThread, fgThread, true); }

        BringWindowToTop(hWnd);
        SetForegroundWindow(hWnd);
        SwitchToThisWindow(hWnd, true);

        if (attached) { AttachThreadInput(myThread, fgThread, false); }

        SpiSet(0x2001, 0, new IntPtr(oldTimeout), 0x02);   // 元に戻す
    }

    // 指定プロセスが持つ「見えていて題名のある窓」を1つ返す（＝エディタの窓）
    public static IntPtr FindMainWindow(int[] pids)
    {
        HashSet<uint> set = new HashSet<uint>();
        foreach (int p in pids) { set.Add((uint)p); }

        IntPtr found = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hWnd, IntPtr lParam)
        {
            if (!IsWindowVisible(hWnd)) { return true; }
            uint pid;
            GetWindowThreadProcessId(hWnd, out pid);
            if (!set.Contains(pid)) { return true; }
            StringBuilder sb = new StringBuilder(256);
            if (GetWindowTextW(hWnd, sb, 256) > 0) { found = hWnd; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
'@

# 高DPI宣言は「窓を作る前」でないと効かない。
# 宣言しないと Windows が勝手に拡大表示して、文字がにじんだ窓になる。
try { [SkrWin]::SetProcessDPIAware() | Out-Null } catch {}

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
# 🔥 前面を奪わない。更新のたびに窓が手前へ飛び出してくるのが邪魔だ、と本人から指摘（2026-08-28）。
#    最前面固定もやめる（作業中のアプリの上に居座らない）。
$form.TopMost         = $false

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

# Show() だと焦点を奪うので、SW_SHOWNOACTIVATE(4) で「出すだけ」にする
$null = $form.Handle                     # ハンドルを先に作らせる
[SkrWin]::ShowWindow($form.Handle, 4) | Out-Null
[System.Windows.Forms.Application]::DoEvents()

function Set-Status([string]$main, [string]$sub) {
    if ($main) { $labelMain.Text = $main }
    $labelSub.Text = $sub
    [System.Windows.Forms.Application]::DoEvents()
}

# 更新は自分の目で追えない（アプリが落ちている最中に動く）ので、あとで追えるよう記録を残す
$logPath = Join-Path $DstDir 'update.log'
function Write-Log([string]$msg) {
    try {
        $line = ('{0}  {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $msg)
        Add-Content -Path $logPath -Value $line -Encoding UTF8
    } catch {}
}
Write-Log '--- 更新を開始 ---'
Write-Log ('進捗窓を表示（DPI {0}%）' -f [int]($scale * 100))

try {
    # --- 1. アプリが終わるのを待つ ------------------------------------------
    Set-Status '更新しています...' 'アプリの終了を待っています'
    Write-Log 'アプリの終了を待っています'
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
    $exclude = @('sakura.ini', 'update-check.txt', 'update.log')
    $files = @(Get-ChildItem $SrcDir -Recurse -File |
               Where-Object { $exclude -notcontains $_.Name -and $_.Name -notlike 'sakura.ini.*' })

    $bar.Style = 'Continuous'          # 件数が分かったので、ここから本物の進捗にする
    $bar.Minimum = 0
    $bar.Maximum = [Math]::Max(1, $files.Count)
    $bar.Value   = 0
    Set-Status '更新しています...' ('0 / {0}' -f $files.Count)
    Write-Log ('コピー開始（{0} 件）' -f $files.Count)

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

    Write-Log 'コピー完了'

    # --- 3. 起動し直す（前面には出さない） -----------------------------------
    # 🔥 以前は ForceForeground で必ず手前に出していた（裏で立ち上がると更新に気づけないため）。
    #    が、更新のたびに窓が前へ飛び出してくるのが邪魔だと本人から指摘があったので、
    #    静かに立ち上げるだけにした（2026-08-28）。更新されたかどうかは
    #    タイトルバーの版番号と update.log で分かる。
    Set-Status '起動しています...' ''
    Start-Sleep -Milliseconds 400
    if (Test-Path $exe) {
        # 最小化で起こす＝前面を奪えない。タスクバーで点滅するので気づける
        Start-Process $exe -WindowStyle Minimized | Out-Null
        Write-Log 'アプリを起動し直した（前面化なし）'
    }
    Start-Sleep -Milliseconds 400
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
    Write-Log '--- 更新を終了 ---'
    $form.Close()
    $form.Dispose()
}
