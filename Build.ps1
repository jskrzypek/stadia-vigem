[CmdletBinding()]
param (
    [Parameter()][ValidateSet("DEBUG", "RELEASE")][string]$Configuration = "DEBUG",
    [Parameter()][ValidateSet("x86", "x64")][string]$Architecture = "x86"
    [Parameter()][bool]$SkipBuildToolsSetup = False
)

$script:CommonFlags = @("/Zi", "/W4", "/EHsc", "/DWIN32", "/D_UNICODE", "/DUNICODE")
$script:DebugFlags = @( "/Od" )
$script:ReleaseFlags = @("/GL", "/O2")

$script:OutputName = "stadia-vigem-"

function Import-Prerequisites {
    if (Get-Module -ListAvailable -Name VSSetup) {
        Update-Module -Name VSSetup
    } else {
        Install-Module -Name VSSetup -Scope CurrentUser -Force
    }

    Import-Module -Name VSSetup

    if (Get-Module -ListAvailable -Name WintellectPowerShell) {
        Update-Module -Name WintellectPowerShell
    } else {
        Install-Module -Name WintellectPowerShell -Scope CurrentUser -Force
    }

    Import-Module -Name WintellectPowerShell
}

function Invoke-BuildTools {
    param (
        $Architecture
    )
    
    if ($SkipBuildToolsSetup -eq True) {
        return
    }

    $latestVsInstallationInfo = Get-VSSetupInstance -All | Sort-Object -Property InstallationVersion -Descending | Select-Object -First 1
    
    Invoke-CmdScript "$($latestVsInstallationInfo.InstallationPath)\VC\Auxiliary\Build\vcvarsall.bat" $Architecture
}

function Invoke-Build {
    param (
        $Architecture
    )

    $OutputName = "$script:OutputName$Architecture.exe"
    $Flags = If ($Configuration -eq "DEBUG") {$script:DebugFlags} else {$script:ReleaseFlags}

    $StopWatch = New-Object -TypeName System.Diagnostics.Stopwatch

    Write-Host "*** ${OutputName}: Build started ***"
    Write-Host

    $StopWatch.Start()

    & "rc.exe" /foobj/stadia-vigem.res res/res.rc
    & "cl.exe" $Flags $CommonFlags /IViGEmClient/include /Foobj/ /Febin/$OutputName ViGEmClient/src/*.cpp obj/stadia-vigem.res src/*.c

    $StopWatch.Stop()

    Write-Host
    Write-Host "*** ${OutputName}: Build finished in $($StopWatch.Elapsed) ***"
}

# Entry

New-Item -Path "bin" -ItemType Directory -Force > $null
New-Item -Path "obj" -ItemType Directory -Force > $null

Import-Prerequisites

Write-Host "-- Build started. Configuration: $Configuration, Architecture: $Architecture --"

if ($Architecture -eq "x86" -Or $Architecture -eq "ALL") {
    Invoke-BuildTools -Architecture "x86"
    Invoke-Build -Architecture "x86"
}

if ($Architecture -eq "x64" -Or $Architecture -eq "ALL") {
    Invoke-BuildTools -Architecture "x64"
    Invoke-Build -Architecture "x64"
}

Write-Host "-- Build completed. --"
