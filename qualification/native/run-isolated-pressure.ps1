# Requires administrative privileges for creation/attachment of one new VHD.
param([Parameter(Mandatory=$true)][string]$TestExecutable,
      [Parameter(Mandatory=$true)][string]$ReportDirectory)
$ErrorActionPreference = 'Stop'
$exe = (Resolve-Path -LiteralPath $TestExecutable).Path
$report = [IO.Path]::GetFullPath($ReportDirectory)
if ((Test-Path -LiteralPath $report) -or !(Test-Path -LiteralPath ([IO.Path]::GetDirectoryName($report)) -PathType Container)) { throw 'A new report directory with an existing parent is required' }
$nonce = [guid]::NewGuid().ToString('N')
$fixture = Join-Path ([IO.Path]::GetTempPath()) "anaiero-pressure-$nonce"
$vhd = Join-Path $fixture 'pressure.vhd'
$letter = $null
foreach ($candidate in 'R','S','T','U','V','W','X','Y','Z') {
  if (!(Test-Path -LiteralPath "${candidate}:\") -and !(Get-PSDrive -Name $candidate -ErrorAction SilentlyContinue)) { $letter = $candidate; break }
}
if (!$letter) { throw 'No unused fixture drive letter is available' }
[void](New-Item -ItemType Directory -Path $fixture -ErrorAction Stop)
$create = Join-Path $fixture 'create.txt'
$diskpart = Join-Path $env:SystemRoot 'System32\diskpart.exe'
$qualificationError = $null
try {
  # Fixed 64 MiB virtual media. Every diskpart operation is scoped to the
  # file we created in this invocation. Never select a disk by numeric ID.
  [IO.File]::WriteAllLines($create,@("create vdisk file=`"$vhd`" maximum=64 type=fixed", "select vdisk file=`"$vhd`"", 'attach vdisk', 'create partition primary', 'format fs=ntfs quick label=ANAIERO_PRESSURE', "assign letter=$letter"),[Text.Encoding]::ASCII)
  & $diskpart /s $create
  if ($LASTEXITCODE -ne 0) { throw "Could not provision isolated NTFS VHD ($LASTEXITCODE)" }
  $root = "${letter}:\"
  $volume = Get-Volume -DriveLetter $letter
  if ($volume.FileSystemLabel -ne 'ANAIERO_PRESSURE' -or $volume.FileSystem -ne 'NTFS' -or $volume.Size -gt 67108864 -or $volume.Size -lt 8388608) { throw 'Provisioned VHD identity or capacity differs; refusing to write' }
  [IO.File]::WriteAllText((Join-Path $root 'OWNER'),"ANAIERO-STORAGE-PRESSURE/1`nnonce=$nonce`n",[Text.UTF8Encoding]::new($false))
  & $exe $root $nonce $report
  if ($LASTEXITCODE -ne 0) { throw "Pressure qualification failed ($LASTEXITCODE); retain any report" }
} catch {
  $qualificationError = $_
  throw
} finally {
  try {
    if (Test-Path -LiteralPath $vhd -PathType Leaf) {
      $image = Get-DiskImage -ImagePath $vhd -ErrorAction Stop
      if ($image.Attached -eq $true) {
        Dismount-DiskImage -ImagePath $vhd -Confirm:$false -ErrorAction Stop
        $image = Get-DiskImage -ImagePath $vhd -ErrorAction Stop
      }
      if ($image.Attached -ne $false) { throw "Cannot verify detachment of $vhd" }
      Remove-Item -LiteralPath $vhd -ErrorAction Stop
    }
    if (Test-Path -LiteralPath $create -PathType Leaf) { Remove-Item -LiteralPath $create -ErrorAction Stop }
    [IO.Directory]::Delete($fixture,$false)
  } catch {
    $cleanupError = $_.Exception
    $message = "Cleanup failed for $fixture. $($cleanupError.Message)"
    if ($null -ne $qualificationError) {
      $message = "Qualification failed. $($qualificationError.Exception.Message) $message"
      throw [AggregateException]::new($message, [Exception[]]@($qualificationError.Exception, $cleanupError))
    }
    throw [InvalidOperationException]::new($message, $cleanupError)
  }
}
