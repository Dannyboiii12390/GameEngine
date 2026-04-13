@echo off
powershell -NoProfile -Command "Get-ChildItem -Path . -Recurse -File -Include '*.h','*.cpp' | Where-Object { $_.FullName -notmatch 'Engine\\IMGUI' } | Get-Content | Measure-Object -Line"
pause