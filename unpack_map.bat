@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "TOOL=%SCRIPT_DIR%build2\bin\Release\w3x_tool.exe"

if not exist "%TOOL%" (
  echo [ERROR] Tool not found: "%TOOL%"
  echo Build the project first, for example:
  echo   cmake --build build2 --config Release
  pause
  exit /b 1
)

if "%~1"=="" (
  echo Usage:
  echo   unpack_map.bat ^<map.w3x^|map.w3m^> [output_dir]
  echo.
  echo You can also drag a map file onto this bat.
  pause
  exit /b 1
)

set "INPUT=%~1"

if "%~2"=="" (
  for %%I in ("%INPUT%") do set "OUTPUT=%%~dpnI_unpack"
) else (
  set "OUTPUT=%~2"
)

echo [INFO] Input : "%INPUT%"
echo [INFO] Output: "%OUTPUT%"
echo.

"%TOOL%" unpack "%INPUT%" "%OUTPUT%"
set "EXIT_CODE=%ERRORLEVEL%"

echo.
if not "%EXIT_CODE%"=="0" (
  echo [ERROR] Unpack failed with exit code %EXIT_CODE%.
  pause
  exit /b %EXIT_CODE%
)

echo [OK] Unpack finished.
pause
exit /b 0
