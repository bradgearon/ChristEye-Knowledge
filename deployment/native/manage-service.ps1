#Requires -RunAsAdministrator
<#
Install or manage the native Anaiero SCM service. Installation is manual and
never runs as part of the build. The explicit credential must identify the
account that will query the same-user named pipe and own session files.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Install', 'Remove', 'Start', 'Stop')]
    [string] $Action,
    [string] $Executable,
    [string] $Config,
    [string] $StateDirectory,
    [ValidatePattern('^[A-Za-z0-9_-]{1,64}$')]
    [string] $Endpoint = 'anaiero-control',
    [PSCredential] $Credential
)
$ErrorActionPreference = 'Stop'
$serviceName = 'Anaiero'

switch ($Action) {
    'Install' {
        if (Get-Service -Name $serviceName -ErrorAction SilentlyContinue) {
            throw 'Anaiero is already installed. Remove it explicitly before changing its service command.'
        }
        if (-not $Executable -or -not $Config -or -not $StateDirectory -or -not $Credential) {
            throw 'Install requires Executable, Config, StateDirectory and Credential.'
        }
        $exePath = (Resolve-Path -LiteralPath $Executable).Path
        $configPath = (Resolve-Path -LiteralPath $Config).Path
        $statePath = [System.IO.Path]::GetFullPath($StateDirectory)
        if ([System.IO.Path]::GetFileName($exePath) -ine 'ce_colony_runtime.exe') {
            throw 'Executable must be ce_colony_runtime.exe; anaiero.exe is a command dispatcher, not the SCM process.'
        }
        foreach ($part in @($exePath, $configPath, $statePath)) {
            if ($part.Contains('"') -or $part.Contains("`r") -or $part.Contains("`n") -or $part.Contains([char]0)) {
                throw 'Service paths cannot contain quotes, newlines or NUL.'
            }
        }
        if (-not (Test-Path -LiteralPath $statePath)) {
            New-Item -ItemType Directory -Path $statePath | Out-Null
        }
        $arguments = @($exePath, 'service', $Endpoint, $configPath, $statePath)
        $binaryPath = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
        New-Service -Name $serviceName -DisplayName 'Anaiero native simulation session' `
            -BinaryPathName $binaryPath -StartupType Manual -Credential $Credential `
            -Description 'Native C++ simulation session with same-user local control and durable evidence.' | Out-Null
        Write-Output 'Anaiero installed with manual startup. Grant the selected account Log on as a service and access to the configured files, then run this script with -Action Start.'
    }
    'Remove' {
        $service = Get-Service -Name $serviceName
        if ($service.Status -ne 'Stopped') { Stop-Service -Name $serviceName }
        & sc.exe delete $serviceName
        if ($LASTEXITCODE -ne 0) { throw "SCM deletion failed with exit code $LASTEXITCODE." }
    }
    'Start' { Start-Service -Name $serviceName }
    'Stop' { Stop-Service -Name $serviceName }
}
