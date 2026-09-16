param(
  [string]$Port = 'COM12',
  [ValidateRange(1, 60)][int]$Seconds = 15
)
$ErrorActionPreference = 'Stop'
$serial = [System.IO.Ports.SerialPort]::new($Port, 115200,
  [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
try {
  $serial.Open()
  $serial.DiscardInBuffer()
  $timer = [System.Diagnostics.Stopwatch]::StartNew()
  $buffer = ''
  while ($timer.Elapsed.TotalSeconds -lt $Seconds) {
    $buffer += $serial.ReadExisting()
    while (($end = $buffer.IndexOf("`n")) -ge 0) {
      $line = $buffer.Substring(0, $end).TrimEnd("`r")
      $buffer = $buffer.Substring($end + 1)
      Write-Output ('{0,8:F3}s {1}' -f $timer.Elapsed.TotalSeconds, $line)
    }
    Start-Sleep -Milliseconds 20
  }
} finally {
  if ($serial.IsOpen) { $serial.Close() }
  $serial.Dispose()
}
