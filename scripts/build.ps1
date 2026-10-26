param(
    [string]$SdkPath = $env:PICO_SDK_PATH,
    [string]$ToolchainPath = $env:PICO_TOOLCHAIN_PATH,
    [string]$BuildDirectory = 'build'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$picoRoot = Join-Path $env:USERPROFILE '.pico-sdk'

function Find-BuildTool([string]$name, [string]$fallback) {
    $command = Get-Command $name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    if (Test-Path -LiteralPath $fallback) { return $fallback }
    throw "Ferramenta ausente: $name. Instale a extensao Raspberry Pi Pico ou adicione ao PATH."
}

if (!$SdkPath) { $SdkPath = Join-Path $picoRoot 'sdk\2.2.0' }
if (!(Test-Path -LiteralPath (Join-Path $SdkPath 'pico_sdk_init.cmake'))) {
    throw 'Informe -SdkPath ou PICO_SDK_PATH apontando para o Pico SDK 2.2.0.'
}
if (!(Test-Path -LiteralPath (Join-Path $SdkPath 'lib\tinyusb\src\tusb.h'))) {
    throw 'Inicialize o submodulo TinyUSB: git submodule update --init lib/tinyusb (dentro do SDK).'
}
if (!$ToolchainPath) {
    $installedToolchain = Join-Path $picoRoot 'toolchain\14_2_Rel1'
    if (Test-Path -LiteralPath $installedToolchain) { $ToolchainPath = $installedToolchain }
}
$cmake = Find-BuildTool 'cmake' (Join-Path $picoRoot 'cmake\v3.31.5\bin\cmake.exe')
$ninja = Find-BuildTool 'ninja' (Join-Path $picoRoot 'ninja\v1.12.1\ninja.exe')
$buildPath = Join-Path $projectRoot $BuildDirectory
$configureArguments = @(
    '-S', $projectRoot, '-B', $buildPath, '-G', 'Ninja',
    "-DPICO_SDK_PATH=$SdkPath", "-DCMAKE_MAKE_PROGRAM=$ninja", '-DCMAKE_BUILD_TYPE=Release', '-DPICO_BOARD=pico_w'
)
if ($ToolchainPath) { $configureArguments += "-DPICO_TOOLCHAIN_PATH=$ToolchainPath" }
$picotoolPath = Join-Path $picoRoot 'picotool\2.2.0-a4\picotool'
if (Test-Path -LiteralPath (Join-Path $picotoolPath 'picotool.exe')) {
    $configureArguments += "-Dpicotool_DIR=$picotoolPath"
}
& $cmake @configureArguments
if ($LASTEXITCODE -ne 0) { throw 'Falha ao configurar o projeto.' }
& $cmake --build $buildPath --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar o firmware.' }
Write-Output "Firmware: $(Join-Path $buildPath 'vibetar.uf2')"

Write-Output "Receptor: $(Join-Path $buildPath 'vibetar-receiver.uf2')"
