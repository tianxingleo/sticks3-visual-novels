param(
    [ValidateSet('Ports', 'Compile', 'Upload')]
    [string]$Action = 'Compile',
    [string]$Port,

    [ValidateSet('StickStory', 'YuilineStory')]
    [string]$Sketch = 'StickStory',
    [string]$ToolchainRoot
)

$ErrorActionPreference = 'Stop'
if ($Action -eq 'Upload' -and $Port -notmatch '^COM[1-9][0-9]*$') {
    throw 'Specify a serial port with -Port, for example COM6.'
}

if ($Action -eq 'Compile' -and $Sketch -eq 'YuilineStory' -and
    !(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'firmware/YuilineStory/Generated.cpp'))) {
    throw 'Missing YuilineStory/Generated.cpp. See docs/yuiline.md for the asset import steps.'
}

$cliArgs = @()
$installedCli = Get-Command arduino-cli -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
if (!$ToolchainRoot -and $installedCli) {
    $cli = $installedCli.Source
} else {
    if (!$ToolchainRoot) {
        $ToolchainRoot = Join-Path $PSScriptRoot '../dafeiyu4esp32'
    }
    $cli = Join-Path $ToolchainRoot '.arduino\bin\arduino-cli.exe'
    $config = Join-Path $ToolchainRoot 'arduino-cli.yaml'
    if (!(Test-Path -LiteralPath $cli) -or !(Test-Path -LiteralPath $config)) {
        throw 'Arduino CLI not found. Install it on PATH or specify -ToolchainRoot.'
    }
    $cli = (Resolve-Path -LiteralPath $cli).Path
    $config = (Resolve-Path -LiteralPath $config).Path
    $toolchain = Split-Path -Parent $config
    $cliArgs = @('--config-file', $config)
    $env:ARDUINO_DIRECTORIES_DATA = Join-Path $toolchain '.arduino\data'
    $env:ARDUINO_DIRECTORIES_DOWNLOADS = Join-Path $toolchain '.arduino\downloads'
    $env:ARDUINO_DIRECTORIES_USER = Join-Path $toolchain '.arduino\user'
}

$usedLetters = (Get-PSDrive -PSProvider FileSystem).Name
$letter = 'S','T','U','V','W','X','Y','Z' | Where-Object { $_ -notin $usedLetters } | Select-Object -First 1
if (-not $letter) {
    throw 'No free drive letter from S: to Z:.'
}

$drive = "${letter}:"
subst.exe $drive $PSScriptRoot
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Push-Location "$drive\"
try {
    $board = 'esp32:esp32:esp32s3:FlashSize=8M,FlashMode=qio,PSRAM=opi,PartitionScheme=default_8MB,USBMode=hwcdc,CDCOnBoot=cdc'
    $source = Join-Path '.\firmware' $Sketch
    $build = Join-Path '.\.build' $Sketch
    switch ($Action) {
        'Ports' { & $cli @cliArgs board list }
        'Compile' { & $cli @cliArgs compile --fqbn $board --build-path $build $source }
        'Upload' { & $cli @cliArgs upload --fqbn $board --port $Port --input-dir $build --verify $source }
    }
    $arduinoExit = $LASTEXITCODE
}
finally {
    Pop-Location
    subst.exe $drive /D
}
exit $arduinoExit
