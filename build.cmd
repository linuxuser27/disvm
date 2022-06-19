@ECHO OFF
SETLOCAL EnableDelayedExpansion

REM Build file for DisVM

REM Process script arguments
SET BUILD_CONFIG=Debug
SET BUILD_BUILD=TRUE
SET BUILD_CLEAN=FALSE
SET BUILD_GENERATE=FALSE
FOR %%G IN (%*) DO (
    IF "%%G" == "Debug" SET BUILD_CONFIG=Debug
    IF "%%G" == "Release" SET BUILD_CONFIG=Release
    IF "%%G" == "-build" SET BUILD_BUILD=FALSE
    IF "%%G" == "clean" SET BUILD_CLEAN=TRUE
    IF "%%G" == "generate" SET BUILD_GENERATE=TRUE
)

SET SRC=%~dp0
SET OUT=%~dp0artifacts\%BUILD_CONFIG%

ECHO ==========================================
ECHO DisVM dev inner loop
ECHO    BUILD_CONFIG=%BUILD_CONFIG%
ECHO    Source=%SRC%
ECHO    Output=%OUT%
ECHO ==========================================

REM Start timer
SET START_TIME=%TIME%

REM Clean
IF NOT "%BUILD_CLEAN%" == "TRUE" GOTO :SKIP_CLEAN
RMDIR /S /Q "%OUT%"
GOTO :DONE
:SKIP_CLEAN

REM Generate CMake project files
IF NOT "%BUILD_GENERATE%" == "TRUE" GOTO :SKIP_RESTORE
cmake -A Win32 -S %SRC% -B %OUT%
:SKIP_RESTORE

REM Build the product
IF NOT "%BUILD_BUILD%" == "TRUE" GOTO :SKIP_BUILD
cmake --build %OUT% --config %BUILD_CONFIG% --target install
:SKIP_BUILD

:DONE

ECHO:
ECHO %~nx0 runtime:
ECHO     Start: %START_TIME%
ECHO       End: %TIME%
