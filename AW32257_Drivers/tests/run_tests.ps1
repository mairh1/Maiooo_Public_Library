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
    "E:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\arm-none-eabi-gcc\bin",
    "C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\arm-none-eabi-gcc\bin"
)
$rvCandidates = @(
    "E:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC15\bin",
    "C:\MounRiver\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC15\bin"
)

$armBin = Resolve-ToolchainBin -RequestedPath $ArmToolchainBin `
    -EnvironmentName "AW32257_ARM_TOOLCHAIN_BIN" `
    -Candidates $armCandidates `
    -RequiredExecutable "arm-none-eabi-gcc.exe"
$rvBin = Resolve-ToolchainBin -RequestedPath $RvToolchainBin `
    -EnvironmentName "AW32257_RV_TOOLCHAIN_BIN" `
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

$pdfPath = Join-Path $projectRoot "AW32257.pdf"
$expectedPdfSha256 = "845B5A4ADC89922A47A360298B52B579A6F94AE9580101600E2E9FC216294563"
$actualPdfSha256 = (Get-FileHash -LiteralPath $pdfPath -Algorithm SHA256).Hash
if ($actualPdfSha256 -ne $expectedPdfSha256)
{
    throw "AW32257.pdf SHA-256 mismatch: $actualPdfSha256"
}
Write-Host "[PASS] AW32257.pdf SHA-256"

$corePaths = @(
    (Join-Path $projectRoot "aw32257.c"),
    (Join-Path $projectRoot "aw32257.h"),
    (Join-Path $projectRoot "aw32257_regs.h")
)
$forbiddenIncludes = Select-String -Path $corePaths `
    -Pattern '#\s*include\s*[<"](?:main\.h|ch32[^>"]*|wch[^>"]*)[>"]' `
    -CaseSensitive:$false
if ($null -ne $forbiddenIncludes)
{
    $forbiddenIncludes | ForEach-Object { Write-Host $_.Line }
    throw "Portable core contains a forbidden platform include."
}
Write-Host "[PASS] portable-core include boundary"

$tempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$buildDirectory = Join-Path $tempRoot ("aw32257-tests-" + [System.Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $buildDirectory | Out-Null

try
{
    $commonWarnings = @("-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-fno-common")
    $includeArguments = @("-I$projectRoot", "-I$exampleRoot")
    $compileInputs = @(
        (Join-Path $projectRoot "aw32257.c"),
        (Join-Path $scriptRoot "compile_aw32257_header.c"),
        (Join-Path $scriptRoot "compile_aw32257_regs_header.c"),
        (Join-Path $exampleRoot "aw32257_ch32_port_example.c")
    )

    $armTarget = @("-mcpu=cortex-m0", "-mthumb", "-ffreestanding")
    for ($index = 0; $index -lt $compileInputs.Count; $index++)
    {
        $output = Join-Path $buildDirectory ("arm-" + $index + ".o")
        $arguments = $commonWarnings + $armTarget + $includeArguments + `
            @("-c", $compileInputs[$index], "-o", $output)
        Invoke-Checked -Executable $armGcc -Arguments $arguments
    }
    Write-Host "[PASS] ARM Cortex-M0 C99 compile"

    $rvTarget = @("-march=rv32imac_zicsr", "-mabi=ilp32", "-ffreestanding")
    for ($index = 0; $index -lt $compileInputs.Count; $index++)
    {
        $output = Join-Path $buildDirectory ("rv32-" + $index + ".o")
        $arguments = $commonWarnings + $rvTarget + $includeArguments + `
            @("-c", $compileInputs[$index], "-o", $output)
        Invoke-Checked -Executable $rvGcc -Arguments $arguments
    }
    Write-Host "[PASS] RV32IMAC C99 compile"

    $testElf = Join-Path $buildDirectory "test_aw32257.elf"
    $testArguments = $commonWarnings + @("-march=rv32imac_zicsr", "-mabi=ilp32") + `
        $includeArguments + @(
            (Join-Path $projectRoot "aw32257.c"),
            (Join-Path $exampleRoot "aw32257_ch32_port_example.c"),
            (Join-Path $scriptRoot "test_aw32257.c"),
            "-o",
            $testElf
        )
    Invoke-Checked -Executable $rvGcc -Arguments $testArguments
    Invoke-Checked -Executable $rvRun -Arguments @("--model", "RV32IMAC", $testElf)
    Write-Host "[PASS] RV32IMAC simulated unit tests"
}
finally
{
    $resolvedBuildDirectory = [System.IO.Path]::GetFullPath($buildDirectory)
    $tempPrefix = $tempRoot.TrimEnd('\') + '\'
    if ($resolvedBuildDirectory.StartsWith($tempPrefix,
                                            [System.StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $resolvedBuildDirectory).StartsWith("aw32257-tests-"))
    {
        Remove-Item -LiteralPath $resolvedBuildDirectory -Recurse -Force
    }
}

Write-Host "AW32257 validation completed successfully."
