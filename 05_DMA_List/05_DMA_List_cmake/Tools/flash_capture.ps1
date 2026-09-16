param(
  [string]$Port = 'COM12',
  [string]$Probe = '002E00233235510F37333439',
  [ValidateRange(1, 60)][int]$Seconds = 8,
  [string]$Firmware = (Join-Path $PSScriptRoot '../build/debug_GCC_NUCLEO-C542RC/05_DMA_List.elf')
)

$ErrorActionPreference = 'Stop'
$firmwarePath = (Resolve-Path -LiteralPath $Firmware).Path
$serial = [System.IO.Ports.SerialPort]::new($Port, 115200,
  [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
try {
  # Open before reset so the startup log is captured as well.
  $serial.Open()
  $serial.DiscardInBuffer()
  & cube programmer -c port=SWD "sn=$Probe" mode=UR reset=HWrst freq=1000 -w $firmwarePath -v -rst
  if ($LASTEXITCODE -ne 0) { throw 'Flash / verification failed' }
  $timer = [System.Diagnostics.Stopwatch]::StartNew()
  while ($timer.Elapsed.TotalSeconds -lt $Seconds) {
    $text = $serial.ReadExisting()
    if ($text.Length -gt 0) { Write-Output $text }
    Start-Sleep -Milliseconds 50
  }
} finally {
  if ($serial.IsOpen) { $serial.Close() }
  $serial.Dispose()
}
