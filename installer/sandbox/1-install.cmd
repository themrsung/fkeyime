@echo off
rem Full wizard, verbose log, never restarts.
msiexec /i C:\fkeyime\fkeyime.msi /norestart /l*v C:\fkeyime\results\install.log
echo msiexec exit code %ERRORLEVEL% > C:\fkeyime\results\install-exit.txt
echo Install finished with exit code %ERRORLEVEL%. & pause
