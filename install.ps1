# Installs Morpheus for this user (%LOCALAPPDATA%\Programs\Morpheus, Start menu and Desktop shortcuts named "Atelier - Morpheus").
$ErrorActionPreference = 'Stop'
& cmd /c "`"$PSScriptRoot\install.bat`" $args"
exit $LASTEXITCODE
