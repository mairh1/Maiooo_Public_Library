param(
    [Parameter(Mandatory = $true)]
    [string]$ArmGcc,

    [Parameter(Mandatory = $true)]
    [string]$RiscvGcc,

    [Parameter(Mandatory = $true)]
    [string]$Python
)

$ErrorActionPreference = 'Stop'
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildRoot = Join-Path $repoRoot 'tmp\cross-checks'
$armBuild = Join-Path $buildRoot 'arm'
$riscvBuild = Join-Path $buildRoot 'rv32'

New-Item -ItemType Directory -Force $armBuild, $riscvBuild | Out-Null

function Invoke-Checked {
    param(
        [string]$Executable,
        [string[]]$Arguments
    )

    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed ($LASTEXITCODE): $Executable $($Arguments -join ' ')"
    }
}

function Invoke-TargetChecks {
    param(
        [string]$Compiler,
        [string]$OutputDirectory,
        [string[]]$TargetFlags
    )

    $common = @('-std=c99', '-Wall', '-Wextra', '-Werror', '-pedantic',
                '-ffreestanding', '-I', $repoRoot) + $TargetFlags
    $sources = @('wm8978.c',
                 'tests\test_wm8978.c',
                 'tests\header_probe.c',
                 'tests\multi_tu_a.c',
                 'tests\multi_tu_b.c',
                 'tests\multi_tu_main.c',
                 'examples\ch32\wm8978_ch32_i2c_port.c',
                 'examples\ch32\wm8978_ch32_example.c')

    foreach ($source in $sources) {
        $sourcePath = Join-Path $repoRoot $source
        $objectName = ($source -replace '[\\/]', '_') -replace '\.c$', '.o'
        $objectPath = Join-Path $OutputDirectory $objectName
        Invoke-Checked $Compiler ($common + @('-c', $sourcePath, '-o', $objectPath))
    }

    Invoke-Checked $Compiler ($TargetFlags + @(
        '-nostdlib', '-Wl,-e,main',
        (Join-Path $OutputDirectory 'wm8978.o'),
        (Join-Path $OutputDirectory 'tests_test_wm8978.o'),
        '-o', (Join-Path $OutputDirectory 'test_wm8978.elf')
    ))

    Invoke-Checked $Compiler ($TargetFlags + @(
        '-nostdlib', '-Wl,-e,main',
        (Join-Path $OutputDirectory 'wm8978.o'),
        (Join-Path $OutputDirectory 'tests_multi_tu_a.o'),
        (Join-Path $OutputDirectory 'tests_multi_tu_b.o'),
        (Join-Path $OutputDirectory 'tests_multi_tu_main.o'),
        '-o', (Join-Path $OutputDirectory 'multi_tu.elf')
    ))

    $cxx = $Compiler -replace 'gcc(\.exe)?$', 'g++$1'
    Invoke-Checked $cxx (@('-std=c++11', '-Wall', '-Wextra', '-Werror',
                           '-pedantic', '-ffreestanding', '-I', $repoRoot) +
                          $TargetFlags + @('-c',
                          (Join-Path $repoRoot 'tests\header_probe.cpp'),
                          '-o', (Join-Path $OutputDirectory 'header_probe_cpp.o')))
}

function Invoke-Rv32BehaviorCheck {
    param(
        [string]$Compiler,
        [string]$OutputDirectory
    )

    $flags = @('-std=c99', '-Wall', '-Wextra', '-Werror', '-pedantic',
               '-ffreestanding', '-march=rv32i', '-mabi=ilp32',
               '-mno-relax', '-I', $repoRoot)
    $coreObject = Join-Path $OutputDirectory 'wm8978_rv32i.o'
    $testObject = Join-Path $OutputDirectory 'test_wm8978_rv32i.o'
    $testElf = Join-Path $OutputDirectory 'test_wm8978_rv32i.elf'

    Invoke-Checked $Compiler ($flags + @(
        '-c', (Join-Path $repoRoot 'wm8978.c'), '-o', $coreObject
    ))
    Invoke-Checked $Compiler ($flags + @(
        '-c', (Join-Path $repoRoot 'tests\test_wm8978.c'), '-o', $testObject
    ))
    Invoke-Checked $Compiler @(
        '-march=rv32i', '-mabi=ilp32', '-nostdlib',
        '-Wl,--no-relax', '-Wl,-e,main',
        $coreObject, $testObject, '-o', $testElf
    )
    Invoke-Checked $Python @(
        (Join-Path $repoRoot 'tests\run_rv32i_test.py'), $testElf
    )
}

Invoke-Checked $Python @((Join-Path $repoRoot 'tests\test_source_contract.py'))
Invoke-TargetChecks $ArmGcc $armBuild @('-mcpu=cortex-m3', '-mthumb')
Invoke-TargetChecks $RiscvGcc $riscvBuild @('-march=rv32imac', '-mabi=ilp32')
Invoke-Rv32BehaviorCheck $RiscvGcc $riscvBuild

Write-Output 'CROSS CHECKS: PASS (contract, RV32I behavior, ARM/RV32 compile/link)'
