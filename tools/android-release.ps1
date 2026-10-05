<#
.SYNOPSIS
    Builds a signed release APK (and optionally the AAB) for BalCalc.

.DESCRIPTION
    Signing settings live outside the repository, in
    %APPDATA%\BalCalc\android-signing.json: the keystore path, the key alias
    and the keystore password encrypted with Windows DPAPI (readable only by
    the current Windows user on this PC).

    First run (or -Setup) creates the keystore with keytool if it does not
    exist and stores the settings. Later runs build without questions.

.EXAMPLE
    .\tools\android-release.ps1 -Setup      # once: create/remember the key
    .\tools\android-release.ps1             # signed APK
    .\tools\android-release.ps1 -Aab        # signed APK + Google Play bundle
    .\tools\android-release.ps1 -Install    # signed APK, then adb install
#>
[CmdletBinding()]
param(
    [switch]$Setup,
    [switch]$Aab,
    [switch]$Install
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$configDir = Join-Path $env:APPDATA 'BalCalc'
$configFile = Join-Path $configDir 'android-signing.json'

# A terminal opened before the SDK was installed lacks these: take them from
# the user/machine environment.
foreach ($n in 'JAVA_HOME', 'ANDROID_SDK_ROOT', 'ANDROID_NDK_ROOT') {
    if (-not [Environment]::GetEnvironmentVariable($n, 'Process')) {
        $v = [Environment]::GetEnvironmentVariable($n, 'User')
        if (-not $v) { $v = [Environment]::GetEnvironmentVariable($n, 'Machine') }
        if ($v) { [Environment]::SetEnvironmentVariable($n, $v, 'Process') }
    }
}

function Find-Tool([string]$name, [string[]]$dirs) {
    foreach ($d in $dirs) {
        if ($d -and (Test-Path (Join-Path $d $name))) { return (Join-Path $d $name) }
    }
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    throw "$name not found (set JAVA_HOME / ANDROID_SDK_ROOT)"
}

function Get-PlainText([securestring]$s) {
    return [Runtime.InteropServices.Marshal]::PtrToStringBSTR(
        [Runtime.InteropServices.Marshal]::SecureStringToBSTR($s))
}

function Invoke-Setup {
    $keytool = Find-Tool 'keytool.exe' @("$env:JAVA_HOME\bin",
        'C:\Program Files\Android\Android Studio\jbr\bin')
    $default = Join-Path $env:USERPROFILE 'balcalc-release.keystore'
    $path = Read-Host "Keystore file [$default]"
    if (-not $path) { $path = $default }
    $alias = Read-Host 'Key alias [balcalc]'
    if (-not $alias) { $alias = 'balcalc' }

    if (Test-Path $path) {
        $pass = Read-Host 'Password of the existing keystore' -AsSecureString
    } else {
        $pass = Read-Host 'New keystore password (6+ characters)' -AsSecureString
        $again = Read-Host 'Repeat the password' -AsSecureString
        if ((Get-PlainText $pass) -ne (Get-PlainText $again)) { throw 'Passwords differ' }
        $name = Read-Host 'Your name or organisation for the certificate [BalCalc]'
        if (-not $name) { $name = 'BalCalc' }
        $env:BALCALC_KS_PASS = Get-PlainText $pass
        try {
            & $keytool -genkeypair -keystore $path -storetype PKCS12 -alias $alias `
                -keyalg RSA -keysize 4096 -validity 10000 -dname "CN=$name" `
                -storepass:env BALCALC_KS_PASS -keypass:env BALCALC_KS_PASS
            if ($LASTEXITCODE -ne 0) { throw 'keytool failed' }
        } finally { Remove-Item Env:BALCALC_KS_PASS -ErrorAction SilentlyContinue }
        Write-Host "Created $path - back it up: updates must be signed with the same key." -ForegroundColor Yellow
    }

    # Check that the password opens the keystore before remembering it.
    $env:BALCALC_KS_PASS = Get-PlainText $pass
    try {
        & $keytool -list -keystore $path -alias $alias -storepass:env BALCALC_KS_PASS | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Wrong password or alias' }
    } finally { Remove-Item Env:BALCALC_KS_PASS -ErrorAction SilentlyContinue }

    New-Item -ItemType Directory -Force $configDir | Out-Null
    [ordered]@{
        keystore = (Resolve-Path $path).Path
        alias    = $alias
        password = ConvertFrom-SecureString $pass   # DPAPI, current user only
    } | ConvertTo-Json | Set-Content -Encoding UTF8 $configFile
    Write-Host "Signing settings saved to $configFile"
}

if ($Setup -or -not (Test-Path $configFile)) { Invoke-Setup }

$cfg = Get-Content $configFile -Raw | ConvertFrom-Json
$env:QT_ANDROID_KEYSTORE_PATH = $cfg.keystore
$env:QT_ANDROID_KEYSTORE_ALIAS = $cfg.alias
$env:QT_ANDROID_KEYSTORE_STORE_PASS = Get-PlainText (ConvertTo-SecureString $cfg.password)
$env:QT_ANDROID_KEYSTORE_KEY_PASS = $env:QT_ANDROID_KEYSTORE_STORE_PASS

try {
    Push-Location $repo
    cmake --preset android-arm64
    if ($LASTEXITCODE -ne 0) { throw 'configure failed' }
    # Ninja does not notice a changed key: drop the packaging stamp and the old
    # outputs so androiddeployqt packages and signs again on every run.
    $buildDir = Join-Path $repo 'build\android-arm64\app\android-build'
    Remove-Item -Force -ErrorAction SilentlyContinue "$buildDir\balcalc.apk",
        "$buildDir\build\outputs\apk\release\*.apk*", "$buildDir\build\outputs\bundle\release\*.aab"
    cmake --build --preset android-arm64
    if ($LASTEXITCODE -ne 0) { throw 'APK build failed' }
    if ($Aab) {
        cmake --build build/android-arm64 --target aab
        if ($LASTEXITCODE -ne 0) { throw 'AAB build failed' }
    }
} finally {
    Pop-Location
    'QT_ANDROID_KEYSTORE_STORE_PASS', 'QT_ANDROID_KEYSTORE_KEY_PASS' |
        ForEach-Object { Remove-Item "Env:$_" -ErrorAction SilentlyContinue }
}

$outputs = Join-Path $repo 'build\android-arm64\app\android-build\build\outputs'
$apk = Get-ChildItem "$outputs\apk\release\*-signed.apk" | Select-Object -First 1
if (-not $apk) { throw "No signed APK in $outputs\apk\release" }

$apksigner = Find-Tool 'apksigner.bat' @("$env:ANDROID_SDK_ROOT\build-tools\35.0.0")
& $apksigner verify --print-certs $apk.FullName | Select-Object -First 3
if ($LASTEXITCODE -ne 0) { throw 'Signature check failed' }

Write-Host "`nSigned APK: $($apk.FullName)" -ForegroundColor Green
if ($Aab) { Get-ChildItem "$outputs\bundle\release\*.aab" | ForEach-Object { Write-Host "AAB: $($_.FullName)" -ForegroundColor Green } }

if ($Install) {
    $adb = Find-Tool 'adb.exe' @("$env:ANDROID_SDK_ROOT\platform-tools")
    & $adb install -r $apk.FullName
}
