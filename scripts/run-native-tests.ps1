[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"


function Find-PlatformIO
{
    $command =
        Get-Command `
            -Name "pio" `
            -CommandType Application `
            -ErrorAction SilentlyContinue

    if ($null -ne $command)
    {
        return $command.Source
    }

    $defaultPath =
        Join-Path `
            $HOME `
            ".platformio\penv\Scripts\pio.exe"

    if (Test-Path -LiteralPath $defaultPath)
    {
        return $defaultPath
    }

    throw @"
PlatformIO Core was not found.

Add PlatformIO to PATH or install the PlatformIO VS Code extension.

Expected fallback path:
$defaultPath
"@
}


$platformIO =
    Find-PlatformIO

$testEnvironments = @(
    "native_button",
    "native_hx711",
    "native_level_indicator",
    "native_operation_indicator",
    "native_scale",
    "native_app",
    "native_tare_record",
    "native_tare_storage",
    "native_calibration_storage",
    "native_console",
    "native_time_delay"
)

$completedSuites = 0
$totalSuites = $testEnvironments.Count


Write-Host ""
Write-Host "Load Cell Level Indicator"
Write-Host "Native regression"
Write-Host "PlatformIO: $platformIO"
Write-Host ""


foreach ($environment in $testEnvironments)
{
    Write-Host (
        "[{0}/{1}] Running {2}" -f
        ($completedSuites + 1),
        $totalSuites,
        $environment
    )

    & $platformIO `
        test `
        -e $environment

    $testExitCode =
        $LASTEXITCODE

    if ($testExitCode -ne 0)
    {
        Write-Host ""
        [Console]::Error.WriteLine(
            "Native regression failed in environment " +
            "'$environment' with exit code $testExitCode."
        )

        exit $testExitCode
    }

    ++$completedSuites

    Write-Host ""
}


Write-Host "Native regression completed successfully."
Write-Host "Suites passed: $completedSuites/$totalSuites"
Write-Host "Expected project test inventory: 249 tests"
Write-Host ""

exit 0
