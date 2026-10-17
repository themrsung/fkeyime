@echo off
msiexec /x C:\fkeyime\fkeyime.msi /qb /norestart /l*v C:\fkeyime\results\uninstall.log
echo msiexec exit code %ERRORLEVEL% > C:\fkeyime\results\uninstall-exit.txt
powershell -NoProfile -ExecutionPolicy Bypass -File C:\fkeyime\check.ps1 after-uninstall
echo Uninstall finished. & pause
