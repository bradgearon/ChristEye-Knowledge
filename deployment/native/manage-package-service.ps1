[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][ValidateSet('Install','Render','Switch','Reconcile','Remove','Start','Stop','Status')][string]$Action,
  [string]$Executable,[string]$StateDirectory,
  [ValidatePattern('^[A-Za-z0-9_-]{1,64}$')][string]$Name='AnaieroModular',
  [ValidatePattern('^[A-Za-z0-9_-]{1,64}$')][string]$Endpoint='anaiero-modular',
  [ValidateRange(1,3600000)][int]$PeriodMilliseconds=100,
  [string]$MeshConfiguration,[string]$Plan,[PSCredential]$Credential,[switch]$IsolatedTest
)
$ErrorActionPreference='Stop'
# Registration authority belongs in an administrator-owned location, not a
# user-writable deployment/state directory. Service accounts only read state.
$settingsDirectory=Join-Path $env:ProgramFiles 'Anaiero\ServiceSettings'
$settingsPath=Join-Path $settingsDirectory ($Name+'.json')
function SafePath([string]$Value) {
  if(-not $Value -or $Value.IndexOfAny([char[]]@(34,13,10,0)) -ge 0){throw 'Service paths cannot be empty or contain quotes, newlines or NUL.'}
  return (Resolve-Path -LiteralPath $Value).Path
}
function RunHost([string]$HostPath,[string[]]$Arguments) {
  & $HostPath @Arguments
  if($LASTEXITCODE -ne 0){throw "Native deployment command failed: $LASTEXITCODE"}
}
function ReadSettings {
  $file=Get-Item -LiteralPath $settingsPath
  if($file.Length -gt 16384 -or ($file.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw 'Invalid bounded service settings.'}
  $value=Get-Content -LiteralPath $settingsPath -Raw | ConvertFrom-Json
  if($value.schema -ne 'anaiero.scm/1' -or $value.name -ne $Name){throw 'Service settings identity differs.'}
  return $value
}
function WriteSettings($Value) {
  New-Item -ItemType Directory -Force -Path $settingsDirectory | Out-Null
  foreach($directory in @((Split-Path -Parent $settingsDirectory), $settingsDirectory)){
    if((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Service settings path traverses a reparse point.'}
  }
  $temporary=Join-Path $settingsDirectory ([guid]::NewGuid().ToString()+'.tmp')
  [IO.File]::WriteAllText($temporary,($Value | ConvertTo-Json -Depth 4),[Text.UTF8Encoding]::new($false))
  if(Test-Path -LiteralPath $settingsPath){
    $backup=$temporary+'.previous'
    [IO.File]::Replace($temporary,$settingsPath,$backup)
    Remove-Item -LiteralPath $backup
  }
  else{[IO.File]::Move($temporary,$settingsPath)}
}
function Selected($Configuration,[string]$Controller) {
  $lines=@(& $Controller package-selected $Configuration.state)
  if($LASTEXITCODE -ne 0 -or $lines.Count -ne 1){throw 'Canonical package selection unavailable.'}
  $selected=SafePath $lines[0]
  if([IO.Path]::GetFileName($selected) -ine 'ce_modular_host.exe'){throw 'Selection must be the native modular host.'}
  RunHost $selected @('check-selected',$Configuration.state) | Out-Null
  return $selected
}
function BinaryPath($Configuration,[string]$Selected) {
  $arguments=@($Selected,'service',$Configuration.endpoint,$Configuration.state,[string]$Configuration.period,'--service-name',$Name)
  if($Configuration.mesh){$arguments+=@('--mesh',$Configuration.mesh)}
  return ($arguments | ForEach-Object {'"'+[regex]::Replace($_,'(\\+)$','$1$1')+'"'}) -join ' '
}
function Stopped {
  $service=Get-Service -Name $Name
  if($service.Status -ne 'Stopped'){throw 'Stop/drain the service before switching its package.'}
  return $service
}
function StopManaged {
  $service=Get-Service -Name $Name
  if($service.Status -ne 'Stopped'){Stop-Service -Name $Name}
  $service.WaitForStatus([ServiceProcess.ServiceControllerStatus]::Stopped,[TimeSpan]::FromSeconds(120))
}
if($Action -eq 'Install' -or $Action -eq 'Render'){
  $controller=SafePath $Executable
  $configuration=[pscustomobject]@{schema='anaiero.scm/1';name=$Name;state=(SafePath $StateDirectory);endpoint=$Endpoint;period=$PeriodMilliseconds;mesh='';executable=$controller}
  if($MeshConfiguration){$configuration.mesh=SafePath $MeshConfiguration}
}else{
  $configuration=ReadSettings
  $controller=if($Executable){SafePath $Executable}else{SafePath $configuration.executable}
}
if($Action -eq 'Render'){Write-Output (BinaryPath $configuration (Selected $configuration $controller));return}
$principal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if(-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'SCM lifecycle changes require an elevated shell.'}
$mutex=[Threading.Mutex]::new($false,('Global\AnaieroPackageLifecycle-'+$Name))
if(-not $mutex.WaitOne(0)){$mutex.Dispose();throw 'Another lifecycle operation owns this service.'}
try{
  switch($Action){
    'Install'{
      $existing=Get-Service -Name $Name -ErrorAction SilentlyContinue
      if(Test-Path -LiteralPath $settingsPath){
        $retained=ReadSettings
        foreach($field in @('name','state','endpoint','period','mesh')){
          if($retained.$field -ne $configuration.$field){throw 'Interrupted install settings differ; review its original declaration.'}
        }
      }elseif($existing){throw 'Refusing to replace a service without this helper''s retained settings.'}
      $isolated=$IsolatedTest -and $env:GITHUB_ACTIONS -eq 'true' -and $env:ANAIERO_REQUIRE_SUPERVISOR -eq '1' -and $Name.StartsWith('AnaieroPackageFixture')
      if(-not $Credential -and -not $isolated){throw 'Install requires an explicit service account Credential.'}
      $selected=Selected $configuration $controller
      $parameters=@{Name=$Name;DisplayName='Anaiero canonical modular session';BinaryPathName=(BinaryPath $configuration $selected);StartupType='Manual';Description='Canonical package selection; retained evidence survives uninstall.'}
      if($Credential){$parameters.Credential=$Credential}
      $configuration.executable=$selected;WriteSettings $configuration
      if($existing){
        Stopped | Out-Null
        $actual=Get-CimInstance Win32_Service -Filter ("Name='"+$Name+"'")
        if($actual.PathName -ne $parameters.BinaryPathName){throw 'Interrupted installation SCM command differs; service remains stopped.'}
      }else{New-Service @parameters | Out-Null}
      Write-Output 'Installed stopped service. Grant its account package/state access before Start.'
    }
    {$_ -eq 'Switch' -or $_ -eq 'Reconcile'}{
      Stopped | Out-Null
      if($Action -eq 'Switch' -and -not $Plan){throw 'Switch requires the immutable reviewed activation plan.'}
      if($Plan){RunHost $controller @('package-activate',$configuration.state,(SafePath $Plan))}
      $selected=Selected $configuration $controller
      $before=Get-CimInstance Win32_Service -Filter ("Name='"+$Name+"'")
      $binaryPath=BinaryPath $configuration $selected
      $changed=Invoke-CimMethod -InputObject $before -MethodName Change -Arguments @{PathName=$binaryPath}
      if($changed.ReturnValue -ne 0){throw "SCM configuration failed: $($changed.ReturnValue)"}
      $after=Get-CimInstance Win32_Service -Filter ("Name='"+$Name+"'")
      if($after.StartName -ne $before.StartName -or $after.PathName -ne $binaryPath){throw 'SCM command/account verification failed; service remains stopped.'}
      $configuration.executable=$selected;WriteSettings $configuration
      Write-Output 'Reconciled stopped SCM command to canonical selection; use Start.'
    }
    'Start'{
      Stopped | Out-Null
      $selected=Selected $configuration $controller
      $actual=Get-CimInstance Win32_Service -Filter ("Name='"+$Name+"'")
      if($actual.PathName -ne (BinaryPath $configuration $selected)){throw 'SCM command differs; run stopped Reconcile.'}
      Start-Service -Name $Name
      (Get-Service -Name $Name).WaitForStatus([ServiceProcess.ServiceControllerStatus]::Running,[TimeSpan]::FromSeconds(30))
    }
    'Stop'{StopManaged}
    'Status'{Get-CimInstance Win32_Service -Filter ("Name='"+$Name+"'") | Select-Object Name,State,StartName,PathName}
    'Remove'{
      StopManaged
      & sc.exe delete $Name
      if($LASTEXITCODE -ne 0){throw "SCM deletion failed: $LASTEXITCODE"}
      Remove-Item -LiteralPath $settingsPath
      Write-Output 'Removed registration. Packages and canonical evidence were preserved.'
    }
  }
}finally{$mutex.ReleaseMutex();$mutex.Dispose()}
