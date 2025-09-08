@echo off

IF "%~1"=="" GOTO usage;
IF "%~2"=="" GOTO usage;

set allargs=%*
CALL SET rest=%%allargs:*%2=%%

SET iterations=%1
SET benchmark_name=%2
SET profiler_dll=../../Release/ProfilerLib.dll

del /S /Q traces

SCTBenchmarksRunner\bin\Release\net48\SCTBenchmarksRunner.exe -s SCTBenchmarks\bin\Release\Benchmarks.exe -p %profiler_dll% -i %iterations% %rest% %benchmark_name%

EXIT /B 0

:usage
    echo Usage: %~nx0 iterations BenchmarkName
    EXIT /B 1
