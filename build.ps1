$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $root 'build'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$mainSource = Join-Path $root 'src\main.c'
$supportSource = Join-Path $root 'src\support.c'
$compilerSource = Join-Path $root 'src\compiler.c'
$output = Join-Path $buildDir 'mini-rustc.exe'

gcc -std=c11 -Wall -Wextra -Wpedantic -O2 $mainSource $supportSource $compilerSource -o $output
Write-Host "Built $output"
