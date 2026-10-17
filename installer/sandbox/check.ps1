# Records FKeyIME's state in Windows Sandbox to C:\fkeyime\results\check-<label>-<time>.txt.
param([string] $Label = 'check')
$out = "C:\fkeyime\results\check-$Label-$(Get-Date -f HHmmss).txt"
& {
    "== $Label $(Get-Date)"
    "== Windows: $((Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion').DisplayVersion) $((Get-CimInstance Win32_OperatingSystem).BuildNumber)"
    "== Language list"
    foreach ($l in (Get-WinUserLanguageList).ToArray()) { "  $($l.LanguageTag): $($l.InputMethodTips -join ', ')" }
    "== Override: $((Get-WinDefaultInputMethodOverride).InputMethodTip)"
    "== TSF registration"
    reg query "HKLM\SOFTWARE\Microsoft\CTF\TIP\{66749D2A-1770-43D3-A269-A051B5BC1AAA}" /s 2>&1 | Select-String -NotMatch 'Category|^$'
    "== COM (64 / 32)"
    reg query "HKLM\SOFTWARE\Classes\CLSID\{66749D2A-1770-43D3-A269-A051B5BC1AAA}\InprocServer32" 2>&1
    reg query "HKLM\SOFTWARE\WOW6432Node\Classes\CLSID\{66749D2A-1770-43D3-A269-A051B5BC1AAA}\InprocServer32" 2>&1
    "== Options"; reg query "HKLM\Software\fkeyime" 2>&1
    "== Files"; Get-ChildItem "$env:ProgramFiles\FKeyIME" -Recurse -ErrorAction SilentlyContinue | ForEach-Object { "  $($_.FullName) $($_.Length)" }
    "== Start menu"; Get-ChildItem "$env:ProgramData\Microsoft\Windows\Start Menu\Programs" -Filter 'FKeyIME*' -ErrorAction SilentlyContinue | ForEach-Object { "  $($_.FullName)" }
    "== Data folder"; Get-ChildItem "$env:ProgramData\fkeyime" -ErrorAction SilentlyContinue | ForEach-Object { "  $($_.Name) $($_.Length)" }
    Get-Content "$env:ProgramData\fkeyime\setup.log" -ErrorAction SilentlyContinue
    "== TIP log: what Settings asked FKeyIME for"
    Get-Content "$env:ProgramData\fkeyime\fkeyime.log" -ErrorAction SilentlyContinue |
        Select-String 'SystemSettings|DllGetClassObject|CreateInstance' | Select-Object -Last 30 | ForEach-Object { "  $($_.Line)" }
    "== Settings window (UI Automation, read-only)"
    Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
    $A = [System.Windows.Automation.AutomationElement]; $T = [System.Windows.Automation.TreeScope]
    $windows = $A::RootElement.FindAll($T::Children, [System.Windows.Automation.Condition]::TrueCondition)
    "  top-level windows: " + (($windows | ForEach-Object { "'$($_.Current.Name)'" }) -join ', ')
    foreach ($w in $windows) {
        if ($w.Current.Name -notmatch 'Settings') { continue }
        "  -- window '$($w.Current.Name)'"
        $seen = @{}
        foreach ($e in $w.FindAll($T::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) {
            $n = $e.Current.Name
            if ($n -and -not $seen[$n]) { $seen[$n] = 1; "  $($e.Current.ControlType.ProgrammaticName -replace 'ControlType.','') | $n" }
        }
    }
} *>&1 | Set-Content $out -Encoding utf8
Write-Host "wrote $out"
