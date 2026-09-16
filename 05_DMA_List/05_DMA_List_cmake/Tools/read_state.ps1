param(
  [string]$Probe = '002E00233235510F37333439',
  [string]$Firmware = (Join-Path $PSScriptRoot '../build/debug_GCC_NUCLEO-C542RC/05_DMA_List.elf'),
  [string[]]$Commands = @('p g_app_diagnostics', 'p g_bringup_dma')
)

$ErrorActionPreference = 'Stop'
function Resolve-CubeCommand([string]$Command) {
  $result = & cube --resolve $Command
  foreach ($line in $result) {
    if ($line -match "^Command: '(.*)'$") { return $Matches[1] }
  }
  throw "Cannot resolve Cube command: $Command"
}
$serverPath = Resolve-CubeCommand 'stlink-gdbserver'
$programmerDir = Split-Path (Resolve-CubeCommand 'programmer')
$serverArgs = @('-d', '-g', '-m', '1', '-p', '61234', '-i', $Probe,
                '--frequency', '1000', '-cp', ('"' + $programmerDir + '"'))
$server = Start-Process -FilePath $serverPath -ArgumentList $serverArgs -PassThru -WindowStyle Hidden
try {
  Start-Sleep -Milliseconds 800
  $gdbArgs = @('--batch', (Resolve-Path -LiteralPath $Firmware).Path,
               '-ex', 'set tcp connect-timeout 5', '-ex', 'target remote localhost:61234')
  foreach ($command in $Commands) { $gdbArgs += @('-ex', $command) }
  $gdbArgs += @('-ex', 'detach')
  & cube arm-none-eabi-gdb @gdbArgs
  if ($LASTEXITCODE -ne 0) { throw 'GDB inspection failed' }
} finally {
  # Only stop the helper launched by this invocation, never another debugger.
  if (-not $server.WaitForExit(3000)) { Stop-Process -Id $server.Id }
  $server.Dispose()
}
