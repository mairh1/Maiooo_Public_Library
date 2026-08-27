param(
    [string]$ArmToolchainBin = "",
    [string]$RvToolchainBin = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $scriptRoot ".."))
$exampleRoot = Join-Path $projectRoot "examples\ch32"

function Resolve-ToolchainBin
{
    param(
        [string]$RequestedPath,
        [string]$EnvironmentName,
        [string[]]$Candidates,
        [string]$RequiredExecutable
    )

    $paths = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($RequestedPath))
    {
        $paths.Add($RequestedPath)
    }

    $environmentPath = [System.Environment]::GetEnvironmentVariable($EnvironmentName)
    if (-not [string]::IsNullOrWhiteSpace($environmentPath))
    {
        $paths.Add($environmentPath)
    }

    foreach ($candidate in $Candidates)
    {
        $paths.Add($candidate)
    }

    foreach ($path in $paths)
    {
        if ([string]::IsNullOrWhiteSpace($path))
        {
            continue
        }

        $fullPath = [System.IO.Path]::GetFullPath($path)
        if (Test-Path -LiteralPath (Join-Path $fullPath $RequiredExecutable) -PathType Leaf)
        {
            return $fullPath
        }
    }

    throw "Cannot find $RequiredExecutable. Pass an explicit toolchain bin path or set $EnvironmentName."
}

function Invoke-Checked
{
    param(
        [string]$Executable,
        [string[]]$Arguments
    )

    Write-Host ("[RUN] " + $Executable + " " + ($Arguments -join " "))
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "$Executable exited with code $LASTEXITCODE"
    }
}

$armCandidates = @(
    "D:\Program_Files\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\arm-none-eabi-gcc\bin",
    "E:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\arm-none-eabi-gcc\bin",
    "C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\arm-none-eabi-gcc\bin"
)
$rvCandidates = @(
    "D:\Program_Files\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC15\bin",
    "E:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC15\bin",
    "C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC15\bin"
)

$armBin = Resolve-ToolchainBin -RequestedPath $ArmToolchainBin `
    -EnvironmentName "INA219_ARM_TOOLCHAIN_BIN" `
    -Candidates $armCandidates `
    -RequiredExecutable "arm-none-eabi-gcc.exe"
$rvBin = Resolve-ToolchainBin -RequestedPath $RvToolchainBin `
    -EnvironmentName "INA219_RV_TOOLCHAIN_BIN" `
    -Candidates $rvCandidates `
    -RequiredExecutable "riscv32-wch-elf-gcc.exe"

$armGcc = Join-Path $armBin "arm-none-eabi-gcc.exe"
$rvGcc = Join-Path $rvBin "riscv32-wch-elf-gcc.exe"
$rvRun = Join-Path $rvBin "riscv32-wch-elf-run.exe"
if (-not (Test-Path -LiteralPath $rvRun -PathType Leaf))
{
    throw "Cannot find RV32 simulator: $rvRun"
}

Write-Host "[INFO] toolchain versions"
Invoke-Checked -Executable $armGcc -Arguments @("--version")
Invoke-Checked -Executable $rvGcc -Arguments @("--version")
Invoke-Checked -Executable $rvRun -Arguments @("--version")

# Resolve the datasheet PDF by wildcard so the ASCII-only script stays
# encoding-safe regardless of the copied file name.
$pdfFile = Get-ChildItem -LiteralPath $projectRoot -Filter "*.pdf" |
    Where-Object { -not $_.PSIsContainer } |
    Select-Object -First 1
if ($null -eq $pdfFile)
{
    throw "No datasheet PDF found in $projectRoot"
}
$pdfPath = $pdfFile.FullName
$expectedPdfSha256 = "2C973858ED8290732F2AEC3EFA66230F69FD741C1DD9E888382632BB400E7770"
$actualPdfSha256 = (Get-FileHash -LiteralPath $pdfPath -Algorithm SHA256).Hash
if ($actualPdfSha256 -ne $expectedPdfSha256)
{
    throw "Datasheet PDF SHA-256 mismatch: $actualPdfSha256"
}
Write-Host "[PASS] datasheet PDF SHA-256"

$corePaths = @(
    (Join-Path $projectRoot "ina219.c"),
    (Join-Path $projectRoot "ina219.h"),
    (Join-Path $projectRoot "ina219_conf.h"),
    (Join-Path $projectRoot "ina219_regs.h"),
    (Join-Path $projectRoot "ina219_io.h")
)
$forbiddenIncludes = Select-String -Path $corePaths `
    -Pattern '#\s*include\s*[<"](?:main\.h|ch32[^>"]*|wch[^>"]*|stm32[^>"]*|hal_[^>"]*)[>"]' `
    -CaseSensitive:$false
if ($null -ne $forbiddenIncludes)
{
    $forbiddenIncludes | ForEach-Object { Write-Host $_.Line }
    throw "Portable core contains a forbidden platform include."
}
Write-Host "[PASS] portable-core include boundary"

$tempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$buildDirectory = Join-Path $tempRoot ("ina219-tests-" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $buildDirectory | Out-Null

try
{
    $commonWarnings = @("-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-fno-common")
    $includeArguments = @("-I$projectRoot", "-I$exampleRoot")

    # 1) ARM Cortex-M0 compile: core + CH32 bridge + usage example
    $armTarget = @("-mcpu=cortex-m0", "-mthumb", "-ffreestanding")
    $compileInputs = @(
        (Join-Path $projectRoot "ina219.c"),
        (Join-Path $exampleRoot "ina219_ch32_i2c_port.c"),
        (Join-Path $exampleRoot "ina219_ch32_example.c")
    )
    for ($index = 0; $index -lt $compileInputs.Count; $index++)
    {
        $output = Join-Path $buildDirectory ("arm-" + $index + ".o")
        $arguments = $commonWarnings + $armTarget + $includeArguments + `
            @("-c", $compileInputs[$index], "-o", $output)
        Invoke-Checked -Executable $armGcc -Arguments $arguments
    }
    Write-Host "[PASS] ARM Cortex-M0 C99 compile"

    # 2) RV32IMAC compile: same inputs (Qingke V2/V4 CH32V/X cores)
    $rvTarget = @("-march=rv32imac_zicsr", "-mabi=ilp32", "-ffreestanding")
    for ($index = 0; $index -lt $compileInputs.Count; $index++)
    {
        $output = Join-Path $buildDirectory ("rv32-" + $index + ".o")
        $arguments = $commonWarnings + $rvTarget + $includeArguments + `
            @("-c", $compileInputs[$index], "-o", $output)
        Invoke-Checked -Executable $rvGcc -Arguments $arguments
    }
    Write-Host "[PASS] RV32IMAC C99 compile"

    # 3) Default config: simulated unit tests on RV32
    $testElfDefault = Join-Path $buildDirectory "test_ina219_default.elf"
    $testArguments = $commonWarnings + $rvTarget + $includeArguments + @(
        (Join-Path $projectRoot "ina219.c"),
        (Join-Path $scriptRoot "test_ina219.c"),
        "-o",
        $testElfDefault
    )
    Invoke-Checked -Executable $rvGcc -Arguments $testArguments
    Invoke-Checked -Executable $rvRun -Arguments @("--model", "RV32IMAC", $testElfDefault)
    Write-Host "[PASS] RV32IMAC simulated unit tests (default config)"

    # 4) VERIFY_WRITES=1 config: compile + simulate (covers read-back branch)
    $testElfVerify = Join-Path $buildDirectory "test_ina219_verify.elf"
    $verifyArguments = $commonWarnings + $rvTarget + $includeArguments + @(
        "-DINA219_VERIFY_WRITES=1",
        (Join-Path $projectRoot "ina219.c"),
        (Join-Path $scriptRoot "test_ina219.c"),
        "-o",
        $testElfVerify
    )
    Invoke-Checked -Executable $rvGcc -Arguments $verifyArguments
    Invoke-Checked -Executable $rvRun -Arguments @("--model", "RV32IMAC", $testElfVerify)
    Write-Host "[PASS] RV32IMAC simulated unit tests (VERIFY_WRITES=1)"

    # 5) THREAD_SAFE=1 config: compile check (io lock/unlock contract branch)
    $threadSafeArguments = $commonWarnings + $rvTarget + $includeArguments + @(
        "-DINA219_THREAD_SAFE=1",
        "-c",
        (Join-Path $projectRoot "ina219.c"),
        "-o",
        (Join-Path $buildDirectory "ina219_threadsafe.o")
    )
    Invoke-Checked -Executable $rvGcc -Arguments $threadSafeArguments
    Write-Host "[PASS] THREAD_SAFE=1 compile check"

    # 6) Trim config: trigger/wait + power APIs compiled out
    $trimArguments = $commonWarnings + $rvTarget + $includeArguments + @(
        "-DINA219_USE_TRIGGERED=0",
        "-DINA219_USE_POWER=0",
        "-c",
        (Join-Path $projectRoot "ina219.c"),
        "-o",
        (Join-Path $buildDirectory "ina219_trim.o")
    )
    Invoke-Checked -Executable $rvGcc -Arguments $trimArguments
    Write-Host "[PASS] TRIGGERED=0 POWER=0 trim compile check"
}
finally
{
    $resolvedBuildDirectory = [System.IO.Path]::GetFullPath($buildDirectory)
    $tempPrefix = $tempRoot.TrimEnd('\') + '\'
    if ($resolvedBuildDirectory.StartsWith($tempPrefix,
                                            [System.StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $resolvedBuildDirectory).StartsWith("ina219-tests-"))
    {
        Remove-Item -LiteralPath $resolvedBuildDirectory -Recurse -Force
    }
}

Write-Host "INA219 validation completed successfully."
