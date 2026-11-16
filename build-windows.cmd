@echo off
setlocal
set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
if not defined VCPKG_ROOT (
  echo Set VCPKG_ROOT to your vcpkg install path.
  exit /b 1
)
if not defined CMAKE_PREFIX_PATH (
  echo Set CMAKE_PREFIX_PATH to your Qt install path.
  exit /b 1
)
set "TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
cd /d "%ROOT%"
if not exist build mkdir build
cmake -B build -G "Visual Studio 17 2022" -A x64 ^
  "-DCMAKE_TOOLCHAIN_FILE=%TOOLCHAIN%" ^
  "-DCMAKE_PREFIX_PATH=%CMAKE_PREFIX_PATH%" ^
  -DKPXC_FEATURE_OS_PASSKEYS=ON ^
  -DKPXC_FEATURE_PASSKEYS=ON ^
  -DKPXC_FEATURE_BROWSER=ON ^
  -DKPXC_FEATURE_DOCS=OFF ^
  -DWITH_TESTS=OFF ^
  -DKEEPASSXC_BUILD_TYPE=Snapshot
if errorlevel 1 exit /b 1
cmake --build build --config Release --parallel
exit /b %ERRORLEVEL%
