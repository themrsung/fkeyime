@echo off
rem Open Settings > Time & language > Language & region > (a language) > Language options first.
powershell -NoProfile -ExecutionPolicy Bypass -File C:\fkeyime\check.ps1 %1
echo Saved to C:\fkeyime\results. & pause
