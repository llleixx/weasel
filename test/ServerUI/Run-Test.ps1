param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference = 'Stop'
$path = (Resolve-Path -LiteralPath $Executable).Path
$out = $path + '.stdout.log'
$err = $path + '.stderr.log'
$test = New-Object System.Diagnostics.Process
$test.StartInfo.FileName = $path
$test.StartInfo.UseShellExecute = $false
$test.StartInfo.CreateNoWindow = $true
$test.StartInfo.RedirectStandardOutput = $true
$test.StartInfo.RedirectStandardError = $true
[void]$test.Start()
$stdout = $test.StandardOutput.ReadToEndAsync()
$stderr = $test.StandardError.ReadToEndAsync()
if (!$test.WaitForExit(15000)) {
  $test.Kill()
  throw "Test timed out: $path"
}
$test.WaitForExit()
$stdout.Result | Set-Content -LiteralPath $out
$stderr.Result | Set-Content -LiteralPath $err
$stdout.Result
$stderr.Result
if ($test.ExitCode -ne 0) { throw "Test failed ($($test.ExitCode)): $path" }
