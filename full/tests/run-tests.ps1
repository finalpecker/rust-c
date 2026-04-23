$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $root
$compiler = Join-Path $projectRoot 'build\mini-rustc.exe'

if (-not (Test-Path $compiler)) {
    & (Join-Path $projectRoot 'build.ps1')
}

$successCases = @(
    @{ Name = 'arithmetic'; File = 'tests\samples\arithmetic.rx'; ExitCode = 5 },
    @{ Name = 'precedence'; File = 'tests\samples\precedence.rx'; ExitCode = 11 },
    @{ Name = 'if_else'; File = 'tests\samples\if_else.rx'; ExitCode = 7 },
    @{ Name = 'while_loop'; File = 'tests\samples\while_loop.rx'; ExitCode = 55 },
    @{ Name = 'recursion'; File = 'tests\samples\recursion.rx'; ExitCode = 120 },
    @{ Name = 'shadowing'; File = 'tests\samples\shadowing.rx'; ExitCode = 9 },
    @{ Name = 'bool_logic'; File = 'tests\samples\bool_logic.rx'; ExitCode = 1 },
    @{ Name = 'compare'; File = 'tests\samples\compare.rx'; ExitCode = 1 },
    @{ Name = 'block_expr'; File = 'tests\samples\block_expr.rx'; ExitCode = 10 },
    @{ Name = 'early_return'; File = 'tests\samples\early_return.rx'; ExitCode = 7 },
    @{ Name = 'nested_calls'; File = 'tests\samples\nested_calls.rx'; ExitCode = 20 },
    @{ Name = 'break_continue'; File = 'tests\samples\break_continue.rx'; ExitCode = 31 },
    @{ Name = 'for_loop'; File = 'tests\samples\for_loop.rx'; ExitCode = 7 }
)

$failureCases = @(
    @{ Name = 'duplicate_param'; File = 'tests\samples\duplicate_param.rx' },
    @{ Name = 'unknown_var'; File = 'tests\samples\unknown_var.rx' },
    @{ Name = 'type_mismatch'; File = 'tests\samples\type_mismatch.rx' },
    @{ Name = 'wrong_arity'; File = 'tests\samples\wrong_arity.rx' },
    @{ Name = 'immutable_assignment'; File = 'tests\samples\immutable_assignment.rx' },
    @{ Name = 'break_outside_loop'; File = 'tests\samples\break_outside_loop.rx' },
    @{ Name = 'continue_outside_loop'; File = 'tests\samples\continue_outside_loop.rx' },
    @{ Name = 'for_range_type_error'; File = 'tests\samples\for_range_type_error.rx' }
)

$failures = 0

foreach ($case in $successCases) {
    $path = Join-Path $projectRoot $case.File
    $output = (& $compiler $path 2>$null | Out-String).Trim()
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        Write-Host "FAIL $($case.Name): compiler returned exit code $code"
        $failures++
        continue
    }
    if ($output -ne [string]$case.ExitCode) {
        Write-Host "FAIL $($case.Name): expected output $($case.ExitCode) but got '$output'"
        $failures++
    } else {
        Write-Host "PASS $($case.Name)"
    }
}

foreach ($case in $failureCases) {
    $path = Join-Path $projectRoot $case.File
    & $compiler $path | Out-Null
    $code = $LASTEXITCODE
    if ($code -eq 0) {
        Write-Host "FAIL $($case.Name): expected compilation failure"
        $failures++
    } else {
        Write-Host "PASS $($case.Name)"
    }
}

if ($failures -ne 0) {
    Write-Host "Tests failed: $failures"
    exit 1
}

Write-Host 'All tests passed.'
