@echo off
if not exist "build" mkdir "build"
cd build
cmake ..
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
cmake --build . --config Release
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
echo Build Successful!
pause
