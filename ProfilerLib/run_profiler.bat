@echo off

setlocal

IF "%~1"=="" GOTO usage;
IF "%~2"=="" GOTO usage;
IF "%~3"=="" GOTO usage;

set allargs=%*
CALL SET rest=%%allargs:*%3=%%

SET profiler=%~f1
SET config=%2
SET program=%3

set COR_ENABLE_PROFILING=1
set COR_PROFILER={00000000-0000-0000-0000-000000000000}
set COR_PROFILER_PATH=%profiler%

set CORECLR_ENABLE_PROFILING=1
set CORECLR_PROFILER={00000000-0000-0000-0000-000000000000}
set CORECLR_PROFILER_PATH=%profiler%

set COR_PROFILER_CONFIG_FILE=%config%
set COR_PROFILER_CONSOLE_PROVIDER=../Debug/ConsoleProvider.exe

%program% %rest%

EXIT /B %errorlevel%

:usage
    echo Usage: %~nx0 profiler.dll profiler.conf program [args]
    EXIT /B 1
