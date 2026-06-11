@echo off
setlocal
cd /d "%~dp0"

set "VSDEV=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VSDEV%" (
    echo HATA: vcvars64.bat bulunamadi: "%VSDEV%"
    exit /b 1
)

if "%VCPKG_ROOT%"=="" set "VCPKG_ROOT=C:\vcpkg"

call "%VSDEV%" >nul
if errorlevel 1 (
    echo HATA: vcvars64.bat cagrisi basarisiz.
    exit /b 1
)

if /I "%~1"=="debug" (
    cmake --preset debug || exit /b 1
    cmake --build --preset debug || exit /b 1
) else (
    cmake --preset default || exit /b 1
    cmake --build --preset default || exit /b 1
)

echo.
echo BUILD OK.
echo Exe: %CD%\build\bin\lan_share.exe
endlocal
