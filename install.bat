@echo off
setlocal
cd /d "%~dp0"
rem Installs Morpheus for this user: %LOCALAPPDATA%\Programs\Morpheus, with Start menu and Desktop shortcuts.
rem Run build.bat first.
set "SRC=build\Release"
if exist "Morpheus.exe" set "SRC=."
if not exist "%SRC%\Morpheus.exe" ( echo Build it first: build.bat & exit /b 1 )
tasklist /fi "imagename eq Morpheus.exe" | find /i "Morpheus.exe" >nul
if not errorlevel 1 ( echo Morpheus is open: close it and run this again. & exit /b 1 )
set "INST=%LOCALAPPDATA%\Programs\Morpheus"
if not exist "%INST%" mkdir "%INST%"
xcopy /e /i /q /y "%SRC%\*" "%INST%\" >nul || ( echo Couldn't copy to %INST% & exit /b 1 )
powershell -NoProfile -Command "$s=New-Object -ComObject WScript.Shell; foreach($d in @([Environment]::GetFolderPath('Programs'),[Environment]::GetFolderPath('Desktop'))){ $l=$s.CreateShortcut((Join-Path $d 'Morpheus.lnk')); $l.TargetPath='%INST%\Morpheus.exe'; $l.WorkingDirectory='%INST%'; $l.IconLocation='%INST%\Morpheus.exe,0'; $l.Description='Morpheus: motion graphics and compositing'; $l.Save() }"
echo Installed: %INST%
if /i not "%~1"=="--quiet" echo Start it from the Start menu or the Desktop.
exit /b 0
