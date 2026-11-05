# Checks the Windows package as it ships: its properties, its files, that the
# core library needs no Visual C++ Redistributable, its shortcuts, that the app
# starts from it, makes its database and fills the library, that a newer MSI
# replaces it, and that it uninstalls cleanly.
#   check-msi.ps1 -Msi <the .msi> -Repo <the repository> [-NoInstall]
# Installing takes administrator rights and changes the machine, so it is for a
# throwaway one (CI). -NoInstall needs no rights and leaves the machine alone: it
# stops after the properties and the files, read from an unpacked copy.
param(
    [Parameter(Mandatory)] [string] $Msi,
    [Parameter(Mandatory)] [string] $Repo,
    [switch] $NoInstall
)
$ErrorActionPreference = 'Stop'

$Msi = (Resolve-Path $Msi).Path
$Repo = (Resolve-Path $Repo).Path
$work = Join-Path ([IO.Path]::GetTempPath()) ("check-msi-" + [guid]::NewGuid())
[void](New-Item -ItemType Directory $work)

function Fail($message) {
    [Console]::Error.WriteLine("check-msi: $message")
    exit 1
}
function Call($object, $member, $arguments) {
    $object.GetType().InvokeMember($member, 'InvokeMethod', $null, $object, $arguments)
}

# --- The MSI database ---------------------------------------------------------
$installer = New-Object -ComObject WindowsInstaller.Installer

function Rows($db, $sql, $columns) {
    $view = Call $db 'OpenView' @($sql)
    [void](Call $view 'Execute' $null)
    while ($record = Call $view 'Fetch' $null) {
        $row = @()
        for ($i = 1; $i -le $columns; $i++) {
            $row += $record.GetType().InvokeMember('StringData', 'GetProperty', $null, $record, @([int]$i))
        }
        , $row
    }
    [void](Call $view 'Close' $null)
}
function Run($db, $sql) {
    $view = Call $db 'OpenView' @($sql)
    [void](Call $view 'Execute' $null)
    [void](Call $view 'Close' $null)
}

$readOnly = 0
$db = Call $installer 'OpenDatabase' @($Msi, $readOnly)
$properties = @{}
Rows $db 'SELECT Property, Value FROM Property' 2 | ForEach-Object { $properties[$_[0]] = $_[1] }

# --- Properties ---------------------------------------------------------------
$cmake = Get-Content (Join-Path $Repo 'CMakeLists.txt') -Raw
if ($cmake -notmatch 'project\(ballistics VERSION ([0-9.]+)') { Fail 'no version in CMakeLists.txt' }
$version = $Matches[1]
$gradle = Get-Content (Join-Path $Repo 'kmp/desktopApp/build.gradle.kts') -Raw
if ($gradle -notmatch 'upgradeUuid = "([0-9a-fA-F-]+)"') { Fail 'no upgradeUuid in build.gradle.kts' }
$upgradeCode = '{' + $Matches[1].ToUpper() + '}'

$expected = @{
    ProductName    = 'Holdmark'
    ProductVersion = $version
    Manufacturer   = 'vetalguru'
    UpgradeCode    = $upgradeCode
    ALLUSERS       = '1'
}
foreach ($name in $expected.Keys) {
    if ($properties[$name] -ne $expected[$name]) {
        Fail "$name is '$($properties[$name])', not '$($expected[$name])'"
    }
}
$fileName = "Holdmark-$version-windows-x64-release.msi"
if ((Split-Path $Msi -Leaf) -ne $fileName) { Fail "file name $(Split-Path $Msi -Leaf) is not $fileName" }

# --- Install ------------------------------------------------------------------
function Msiexec($arguments, $what) {
    $log = Join-Path $work "$what.log"
    $process = Start-Process msiexec -Wait -PassThru -ArgumentList ($arguments + @('/qn', '/l*v', "`"$log`""))
    # 3010: fine, a reboot is wanted.
    if ($process.ExitCode -notin 0, 3010) {
        if (Test-Path $log) { Get-Content $log -Tail 40 | ForEach-Object { [Console]::Error.WriteLine($_) } }
        Fail "$what failed with exit code $($process.ExitCode)"
    }
}
function Installed {
    $keys = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*'
    Get-ItemProperty $keys -ErrorAction SilentlyContinue | Where-Object { $_.DisplayName -eq 'Holdmark' }
}

if ($NoInstall) {
    $unpacked = Join-Path $work 'unpacked'
    Msiexec @('/a', "`"$Msi`"", "TARGETDIR=`"$unpacked`"") 'unpack'
    $root = Join-Path $unpacked 'Holdmark'
} else {
    # Never over the developer's own copy.
    if (@(Installed).Count -ne 0) { Fail 'Holdmark is already installed on this machine' }
    Msiexec @('/i', "`"$Msi`"") 'install'
    $root = Join-Path $env:ProgramFiles 'Holdmark'

    $entries = @(Installed)
    if ($entries.Count -ne 1) { Fail "$($entries.Count) entries in Apps & Features after installing" }
    if ($entries[0].DisplayVersion -ne $version) { Fail "installed as version $($entries[0].DisplayVersion)" }
}

# --- Files --------------------------------------------------------------------
$exe = Join-Path $root 'Holdmark.exe'
$res = Join-Path $root 'app\resources'
$dll = Join-Path $res 'balcalc_jni.dll'
if (-not (Test-Path $exe)) { Fail 'no launcher Holdmark.exe' }
if (-not (Test-Path $dll)) { Fail 'no balcalc_jni.dll in the resources' }
if (-not (Test-Path (Join-Path $res 'balcalc.png'))) { Fail 'no window icon in the resources' }

$seedKinds = '.ammo', '.drg', '.reticle', '.json'
$shipped = @(Get-ChildItem (Join-Path $res 'seed') -Recurse -File).Count
$seedRoot = Join-Path $Repo 'data\seed'
$wanted = @(Get-ChildItem $seedRoot -Recurse -File | Where-Object {
        $_.Extension -in $seedKinds -and -not $_.FullName.StartsWith((Join-Path $seedRoot 'sources\'))
    }).Count
if ($shipped -ne $wanted) { Fail "$shipped seed files shipped, data/seed has $wanted" }

# The C++ runtime is linked in (/MT), so no Redistributable is needed: the names
# of its libraries would stand in the import table otherwise.
$text = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($dll))
if ($text -match '(?i)(vcruntime|msvcp)[0-9]+[a-z_0-9]*\.dll') { Fail "balcalc_jni.dll needs $($Matches[0])" }

if ($NoInstall) {
    Write-Output "check-msi: $(Split-Path $Msi -Leaf) is fine, not installed ($shipped seed files)"
    exit 0
}

# --- Shortcuts ----------------------------------------------------------------
$shell = New-Object -ComObject WScript.Shell
$startMenu = Join-Path $env:ProgramData 'Microsoft\Windows\Start Menu\Programs\Holdmark\Holdmark.lnk'
$desktop = Join-Path $env:PUBLIC 'Desktop\Holdmark.lnk'
foreach ($link in $startMenu, $desktop) {
    if (-not (Test-Path $link)) { Fail "no shortcut $link" }
    $target = $shell.CreateShortcut($link).TargetPath
    if ($target -ne $exe) { Fail "shortcut $link starts $target, not $exe" }
}

# --- The app starts -----------------------------------------------------------
function Stop-App {
    Get-Process Holdmark -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    for ($i = 0; $i -lt 60 -and (Get-Process Holdmark -ErrorAction SilentlyContinue); $i++) { Start-Sleep -Milliseconds 500 }
}
function Show-AppLog {
    foreach ($log in $outLog, $errLog) {
        if (Test-Path $log) { Get-Content $log | ForEach-Object { [Console]::Error.WriteLine($_) } }
    }
}

$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) { Fail 'no python, which reads the database' }
$countBullets = @'
import sqlite3, sys
try:
    uri = 'file:' + sys.argv[1].replace(chr(92), '/') + '?mode=ro'
    print(sqlite3.connect(uri, uri=True).execute('select count(*) from bullet').fetchone()[0])
except Exception:
    print(0)
'@
$countScript = Join-Path $work 'count.py'
Set-Content $countScript $countBullets -Encoding ASCII

$dbFile = Join-Path $work 'holdmark.db'
$outLog = Join-Path $work 'app.out.log'
$errLog = Join-Path $work 'app.err.log'
$env:HOLDMARK_DB = $dbFile
try {
    [void](Start-Process $exe -PassThru -RedirectStandardOutput $outLog -RedirectStandardError $errLog)

    # The starter library in the database: the core loaded, the seed read.
    $bullets = 0
    for ($i = 0; $i -lt 120; $i++) {
        if (-not (Get-Process Holdmark -ErrorAction SilentlyContinue)) {
            Show-AppLog
            Fail 'the app quit while starting'
        }
        if (Test-Path $dbFile) {
            $bullets = [int](& $python.Source $countScript $dbFile)
            if ($bullets -gt 0) { break }
        }
        Start-Sleep -Milliseconds 500
    }
    if ($bullets -le 0) {
        Show-AppLog
        Fail "no library in $dbFile after 60 s"
    }
    # Still up a moment later, with nothing thrown.
    Start-Sleep -Seconds 3
    if (-not (Get-Process Holdmark -ErrorAction SilentlyContinue)) {
        Show-AppLog
        Fail 'the app quit after starting'
    }
    if ((Get-Content $outLog, $errLog -ErrorAction SilentlyContinue) -match 'Exception') {
        Show-AppLog
        Fail 'the app logged an exception'
    }
} finally {
    Stop-App
}

# --- A newer MSI replaces this one --------------------------------------------
# The same package made one version newer, as the next release would be: what
# matters is that it finds the installed one through the fixed upgradeUuid.
$parsed = [version]$version
$newer = "$($parsed.Major).$($parsed.Minor).$($parsed.Build + 1)"
$newerMsi = [string](Join-Path $work 'newer.msi') # A COM call does not take a cmdlet's wrapped string.
Copy-Item $Msi $newerMsi

$transact = 1
$edit = Call $installer 'OpenDatabase' @($newerMsi, $transact)
Run $edit "UPDATE Property SET Value='$newer' WHERE Property='ProductVersion'"
$productCode = [guid]::NewGuid().ToString('B').ToUpper()
Run $edit "UPDATE Property SET Value='$productCode' WHERE Property='ProductCode'"
# The versions are part of the Upgrade table's key: its rows can only be replaced.
$upgrades = @(Rows $edit 'SELECT UpgradeCode, VersionMin, VersionMax, Attributes, ActionProperty FROM Upgrade' 5)
if ($upgrades.Count -eq 0) { Fail 'no Upgrade table' }
Run $edit 'DELETE FROM Upgrade'
foreach ($row in $upgrades) {
    $columns = 'UpgradeCode', 'Attributes', 'ActionProperty'
    $values = "'$($row[0])'", $row[3], "'$($row[4])'"
    if ($row[1]) { $columns += 'VersionMin'; $values += "'$($row[1] -replace [regex]::Escape($version), $newer)'" }
    if ($row[2]) { $columns += 'VersionMax'; $values += "'$($row[2] -replace [regex]::Escape($version), $newer)'" }
    Run $edit "INSERT INTO Upgrade ($($columns -join ', ')) VALUES ($($values -join ', '))"
}
$summary = $edit.GetType().InvokeMember('SummaryInformation', 'GetProperty', $null, $edit, @([int]1))
$packageCode = 9
$summary.GetType().InvokeMember('Property', 'SetProperty', $null, $summary, @([int]$packageCode, [guid]::NewGuid().ToString('B').ToUpper()))
[void](Call $summary 'Persist' $null)
[void](Call $edit 'Commit' $null)
[void][Runtime.InteropServices.Marshal]::ReleaseComObject($summary)
[void][Runtime.InteropServices.Marshal]::ReleaseComObject($edit)
# The file stays locked until its last COM object is collected, views included.
$edit = $summary = $null
[GC]::Collect()
[GC]::WaitForPendingFinalizers()

Msiexec @('/i', "`"$newerMsi`"") 'upgrade'
$entries = @(Installed)
if ($entries.Count -ne 1) { Fail "$($entries.Count) entries in Apps & Features after upgrading: the old version stayed" }
if ($entries[0].DisplayVersion -ne $newer) { Fail "upgraded to version $($entries[0].DisplayVersion), not $newer" }
if (-not (Test-Path $exe)) { Fail 'no Holdmark.exe after upgrading' }

# --- Uninstall ----------------------------------------------------------------
Msiexec @('/x', "`"$productCode`"") 'uninstall'
if (@(Installed).Count -ne 0) { Fail 'still in Apps & Features after uninstalling' }
foreach ($path in $exe, $startMenu, $desktop) {
    if (Test-Path $path) { Fail "$path is still there after uninstalling" }
}

Write-Output "check-msi: $(Split-Path $Msi -Leaf) is fine ($shipped seed files, $bullets bullets in a new database, upgraded to $newer and uninstalled)"
exit 0
