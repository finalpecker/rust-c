param(
    [ValidateSet('teaching', 'full')]
    [string]$Project = 'teaching'
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Join-Path $root $Project

if (-not (Test-Path $projectRoot)) {
    throw "Project root not found: $projectRoot"
}

& (Join-Path $projectRoot 'build.ps1')
