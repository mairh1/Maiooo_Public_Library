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
    -EnvironmentName "MAX17048_ARM_TOOLCHAIN_BIN" `
    -Candidates $armCandidates `
    -RequiredExecutable "arm-none-eabi-gcc.exe"
$rvBin = Resolve-ToolchainBin -RequestedPath $RvToolchainBin `
    -EnvironmentName "MAX17048_RV_TOOLCHAIN_BIN" `
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

# Resolve the datasheet PDF by wildcard: the file name contains Chinese
# characters, which are not encoding-safe inside this ASCII-only script.
$pdfFile = Get-ChildItem -LiteralPath $projectRoot -Filter "*.PDF" |
    Where-Object { -not $_.PSIsContainer } |
    Select-Object -First 1
if ($null -eq $pdfFile)
{
    throw "No datasheet PDF found in $projectRoot"
}
$pdfPath = $pdfFile.FullName
$expectedPdfSha256 = "70DC8EEF0E012276DCDC58B6DCE64AF08258304BCF865CEACE64E856B8029330"
$actualPdfSha256 = (Get-FileHash -LiteralPath $pdfPath -Algorithm SHA256).Hash
if ($actualPdfSha256 -ne $expectedPdfSha256)
{
    throw "Datasheet PDF SHA-256 mismatch: $actualPdfSha256"
}
Write-Host "[PASS] datasheet PDF SHA-256"

$corePaths = @(
    (Join-Path $projectRoot "max17048.c"),
    (Join-Path $projectRoot "max17048.h"),
    (Join-Path $projectRoot "max17048_conf.h"),
    (Join-Path $projectRoot "max17048_regs.h"),
    (Join-Path $projectRoot "max17048_io.h")
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
$buildDirectory = Join-Path $tempRoot ("max17048-tests-" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $buildDirectory | Out-Null

try
{
    $commonWarnings = @("-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-fno-common")
    $includeArguments = @("-I$projectRoot", "-I$exampleRoot")

    # 1) ARM Cortex-M0 compile: core + CH32 bridge + usage example
    $armTarget = @("-mcpu=cortex-m0", "-mthumb", "-ffreestanding")
    $compileInputs = @(
        (Join-Path $projectRoot "max17048.c"),
        (Join-Path $exampleRoot "max17048_ch32_i2c_port.c"),
        (Join-Path $exampleRoot "max17048_ch32_example.c")
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
    $testElfDefault = Join-Path $buildDirectory "test_max17048_default.elf"
    $testArguments = $commonWarnings + $rvTarget + $includeArguments + @(
        (Join-Path $projectRoot "max17048.c"),
        (Join-Path $scriptRoot "test_max17048.c"),
        "-o",
        $testElfDefault
    )
    Invoke-Checked -Executable $rvGcc -Arguments $testArguments
    Invoke-Checked -Executable $rvRun -Arguments @("--model", "RV32IMAC", $testElfDefault)
    Write-Host "[PASS] RV32IMAC simulated unit tests (default config)"

    # 4) VERIFY_WRITES=1 config: compile + simulate (covers read-back branch)
    $testElfVerify = Join-Path $buildDirectory "test_max17048_verify.elf"
    $verifyArguments = $commonWarnings + $rvTarget + $includeArguments + @(
        "-DMAX17048_VERIFY_WRITES=1",
        (Join-Path $projectRoot "max17048.c"),
        (Join-Path $scriptRoot "test_max17048.c"),
        "-o",
        $testElfVerify
    )
    Invoke-Checked -Executable $rvGcc -Arguments $verifyArguments
    Invoke-Checked -Executable $rvRun -Arguments @("--model", "RV32IMAC", $testElfVerify)
    Write-Host "[PASS] RV32IMAC simulated unit tests (VERIFY_WRITES=1)"

    # 5) THREAD_SAFE=1 config: compile check (io lock/unlock contract branch)
    $threadSafeArguments = $commonWarnings + $rvTarget + $includeArguments + @(
        "-DMAX17048_THREAD_SAFE=1",
        "-c",
        (Join-Path $projectRoot "max17048.c"),
        "-o",
        (Join-Path $buildDirectory "max17048_threadsafe.o")
    )
    Invoke-Checked -Executable $rvGcc -Arguments $threadSafeArguments
    Write-Host "[PASS] THREAD_SAFE=1 compile check"

    # 6) MODEL_TABLE=0 trim config: compile check
    $trimArguments = $commonWarnings + $rvTarget + $includeArguments + @(
        "-DMAX17048_USE_MODEL_TABLE=0",
        "-c",
        (Join-Path $projectRoot "max17048.c"),
        "-o",
        (Join-Path $buildDirectory "max17048_trim.o")
    )
    Invoke-Checked -Executable $rvGcc -Arguments $trimArguments
    Write-Host "[PASS] MODEL_TABLE=0 trim compile check"
}
finally
{
    $resolvedBuildDirectory = [System.IO.Path]::GetFullPath($buildDirectory)
    $tempPrefix = $tempRoot.TrimEnd('\') + '\'
    if ($resolvedBuildDirectory.StartsWith($tempPrefix,
                                            [System.StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $resolvedBuildDirectory).StartsWith("max17048-tests-"))
    {
        Remove-Item -LiteralPath $resolvedBuildDirectory -Recurse -Force
    }
}

Write-Host "MAX17048 validation completed successfully."
