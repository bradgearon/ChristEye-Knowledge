#Requires -RunAsAdministrator
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Install', 'Remove', 'Start', 'Stop')]
    [string] $Action,
    [string] $Executable,
    [string] $StateDirectory,
    [ValidatePattern('^[A-Za-z0-9_-]{1,64}$')]
    [string] $Endpoint = 'anaiero-modular',
    [ValidateRange(1, 3600000)]
    [int] $PeriodMilliseconds = 100,
    [PSCredential] $Credential
)
$ErrorActionPreference = 'Stop'
$serviceName = 'AnaieroModular'
switch ($Action) {
    'Install' {
        if (Get-Service -Name $serviceName -ErrorAction SilentlyContinue) {
            throw 'AnaieroModular is already installed. Stop and remove the service explicitly before changing its binary.'
        }
        if (-not $Executable -or -not $StateDirectory -or -not $Credential) {
            throw 'Install requires Executable, StateDirectory and an explicit service account Credential.'
        }
        $exePath = (Resolve-Path -LiteralPath $Executable).Path
        $statePath = (Resolve-Path -LiteralPath $StateDirectory).Path
        if ([System.IO.Path]::GetFileName($exePath) -ine 'ce_modular_host.exe') {
            throw 'Executable must be ce_modular_host.exe, which owns the native SCM lifecycle.'
        }
        foreach ($part in @($exePath, $statePath)) {
            if ($part.Contains('"') -or $part.Contains("`r") -or $part.Contains("`n") -or $part.Contains([char]0)) {
                throw 'Service paths cannot contain quotes, newlines or NUL.'
            }
        }
        & $exePath check $statePath
        if ($LASTEXITCODE -ne 0) { throw 'The package is incompatible with this retained session, or its store is not quiescent.' }
        $arguments = @($exePath, 'service', $Endpoint, $statePath, "$PeriodMilliseconds")
        $binaryPath = ($arguments | ForEach-Object {
            '"' + [regex]::Replace($_, '(\\+)$', '$1$1') + '"'
        }) -join ' '
        New-Service -Name $serviceName -DisplayName 'Anaiero canonical modular session' `
            -BinaryPathName $binaryPath -StartupType Manual -Credential $Credential `
            -Description 'Native canonical sessions with durable cycles and same-user local control.' | Out-Null
        Write-Output 'Installed with manual startup. Grant the account Log on as a service and access to package/state files before Start.'
    }
    'Start' { Start-Service -Name $serviceName }
    'Stop' { Stop-Service -Name $serviceName }
    'Remove' {
        $service = Get-Service -Name $serviceName
        if ($service.Status -ne 'Stopped') { Stop-Service -Name $serviceName }
        & sc.exe delete $serviceName
        if ($LASTEXITCODE -ne 0) { throw "SCM deletion failed with exit code $LASTEXITCODE." }
        Write-Output 'Removed the service registration. Retained evidence and package files were preserved.'
    }
}
