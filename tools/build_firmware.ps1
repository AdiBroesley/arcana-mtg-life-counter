param([switch]$Configure)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
 $python=Join-Path $projectRoot '.tools\esp-python\Scripts\python.exe'
 if(!(Test-Path -LiteralPath $python)) {
  & py -3.11 -m venv .tools/esp-python
  if($LASTEXITCODE){throw 'Python 3.11 is required to prepare the firmware tools.'}
  & $python -m pip install platformio==6.2.0 esptool==4.9.0
  if($LASTEXITCODE){throw 'Firmware tool installation failed.'}
 }
 $env:PLATFORMIO_CORE_DIR=Join-Path $projectRoot '.tools\platformio'
 # Versions are pinned in the project; skip unrelated component update checks.
 $env:IDF_COMPONENT_CHECK_NEW_VERSION='0'
 if($Configure){& $python -m platformio run -d firmware -t menuconfig}
 else {& $python -m platformio run -d firmware -j 4}
 if($LASTEXITCODE){throw 'Firmware build failed; see compiler output.'}
} finally {Pop-Location}

