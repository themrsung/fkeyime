# M0 spike: install or remove the FKeyIME TIP on this PC for measurements. Needs admin (it
# re-launches itself elevated). Development only; the MSI replaces this in M5.
#   tools/spike.ps1 -Install [-Config Release]   copy, regsvr32 (x64 + x86), add the 3 profiles
#                                                to the language list (stock IMEs stay)
#   tools/spike.ps1 -Uninstall                   undo all of it, restore the language list
#   tools/spike.ps1 -Option Defer=1              set a TIP option (HKLM\Software\fkeyime)
#   tools/spike.ps1 -Log                         tail the TIP log
param(
    [switch] $Install,
    [switch] $Uninstall,
    [string[]] $Option,
    [switch] $Log,
    [ValidateSet('Debug', 'Release')] [string] $Config = 'Release'
)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path "$PSScriptRoot\.."
$dest = "$env:ProgramFiles\FKeyIME-spike"
$data = "$env:ProgramData\fkeyime"
$optionsKey = 'HKLM:\Software\fkeyime'
$clsid = '{66749D2A-1770-43D3-A269-A051B5BC1AAA}'
$tips = [ordered]@{
    'en-US' = "0409:$clsid{8F61A2C0-75BC-4042-B739-CC7D63B542E5}"
    'ko'    = "0412:$clsid{28C201BF-390E-482F-8259-CD26E05844A9}"
    'ja'    = "0411:$clsid{1AC13F3E-07AC-4B03-A606-967BD48BFD99}"
}

if ($Log) {
    Get-Content "$data\fkeyime.log" -Tail 40 -Wait
    return
}

$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).
    IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) {
    $argv = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"", '-Config', $Config)
    if ($Install) { $argv += '-Install' }
    if ($Uninstall) { $argv += '-Uninstall' }
    if ($Option) { $argv += '-Option'; $argv += ($Option -join ',') }
    $p = Start-Process powershell -Verb RunAs -ArgumentList $argv -Wait -PassThru
    exit $p.ExitCode
}

function Save-LanguageList([string] $path) {
    # ToArray: the cmdlet emits the whole List as one object, so loops would see one item.
    $lines = foreach ($l in (Get-WinUserLanguageList).ToArray()) { "$($l.LanguageTag)|$($l.InputMethodTips -join ',')" }
    $lines | Set-Content $path -Encoding utf8
}

# The elevated window closes on exit, so keep a transcript next to the log.
New-Item $data -ItemType Directory -Force | Out-Null
Start-Transcript "$data\spike.ps1.log" -Append | Out-Null
trap { Write-Host $_ -ForegroundColor Red; Write-Host $_.ScriptStackTrace; Stop-Transcript | Out-Null; exit 1 }

if ($Option) {
    New-Item $optionsKey -Force | Out-Null
    foreach ($o in ($Option -join ',').Split(',')) {
        $name, $value = $o.Split('=')
        Set-ItemProperty $optionsKey $name ([int]$value) -Type DWord
        Write-Host "option $name = $value (applies to processes that load the TIP from now on)"
    }
}

if ($Install) {
    # Log folder writable by every process, AppContainer (Store) apps included.
    New-Item $data -ItemType Directory -Force | Out-Null
    icacls $data /grant '*S-1-15-2-1:(OI)(CI)M' '*S-1-5-32-545:(OI)(CI)M' | Out-Null
    if (-not (Test-Path "$data\languagelist.before-spike.txt")) {
        Save-LanguageList "$data\languagelist.before-spike.txt"
    }

    # Program Files is readable by ALL APPLICATION PACKAGES, the user's Desktop isn't.
    foreach ($arch in 'x64', 'x86') {
        New-Item "$dest\$arch" -ItemType Directory -Force | Out-Null
        Copy-Item "$root\build\$arch\src\tip\$Config\fkeyime.dll" "$dest\$arch\" -Force
    }
    & "$env:SystemRoot\System32\regsvr32.exe" /s "$dest\x64\fkeyime.dll"
    if ($LASTEXITCODE) { throw "regsvr32 x64 failed ($LASTEXITCODE)" }
    & "$env:SystemRoot\SysWOW64\regsvr32.exe" /s "$dest\x86\fkeyime.dll"
    if ($LASTEXITCODE) { throw "regsvr32 x86 failed ($LASTEXITCODE)" }

    $list = Get-WinUserLanguageList
    foreach ($tag in $tips.Keys) {
        $lang = $null
        foreach ($l in $list) { if ($l.LanguageTag -eq $tag) { $lang = $l } }
        if (-not $lang) {
            $lang = (New-WinUserLanguageList $tag)[0]
            $lang.InputMethodTips.Clear()
            $list.Add($lang)
        }
        if ($lang.InputMethodTips -notcontains $tips[$tag]) { $lang.InputMethodTips.Add($tips[$tag]) }
    }
    Set-WinUserLanguageList $list -Force
    Write-Host 'Installed. Language list now:'
    foreach ($l in (Get-WinUserLanguageList).ToArray()) { "  $($l.LanguageTag): $($l.InputMethodTips -join ', ')" }
}

if ($Uninstall) {
    $backup = "$data\languagelist.before-spike.txt"
    if (Test-Path $backup) {
        $list = New-WinUserLanguageList (Get-Content $backup | Select-Object -First 1).Split('|')[0]
        $list.Clear()
        foreach ($line in Get-Content $backup) {
            $tag, $saved = $line.Split('|')
            $lang = (New-WinUserLanguageList $tag)[0]
            $lang.InputMethodTips.Clear()
            foreach ($t in $saved.Split(',')) { if ($t) { $lang.InputMethodTips.Add($t) } }
            $list.Add($lang)
        }
        Set-WinUserLanguageList $list -Force
        Remove-Item $backup
    }
    foreach ($arch in 'x64', 'x86') {
        $regsvr = if ($arch -eq 'x64') { 'System32' } else { 'SysWOW64' }
        if (Test-Path "$dest\$arch\fkeyime.dll") {
            & "$env:SystemRoot\$regsvr\regsvr32.exe" /s /u "$dest\$arch\fkeyime.dll"
        }
    }
    # regsvr32 /u leaves the empty TSF category keys and (measured) the 32-bit CLSID key behind.
    foreach ($key in "HKLM:\SOFTWARE\Microsoft\CTF\TIP\$clsid", "HKLM:\SOFTWARE\Classes\CLSID\$clsid",
                     "HKLM:\SOFTWARE\WOW6432Node\Classes\CLSID\$clsid") {
        Remove-Item $key -Recurse -ErrorAction SilentlyContinue
    }
    Remove-Item $optionsKey -Recurse -ErrorAction SilentlyContinue
    # Processes that loaded the DLL keep it open until they exit; delete what we can.
    Remove-Item $dest -Recurse -Force -ErrorAction SilentlyContinue
    if (Test-Path $dest) { Write-Warning "$dest is still in use; delete it after signing out." }
    Write-Host 'Uninstalled. Language list now:'
    foreach ($l in (Get-WinUserLanguageList).ToArray()) { "  $($l.LanguageTag): $($l.InputMethodTips -join ', ')" }
}
Stop-Transcript | Out-Null
