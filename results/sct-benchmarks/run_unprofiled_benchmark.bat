@echo off

IF "%~1"=="" GOTO usage;
IF "%~2"=="" GOTO usage;

SET iterations=%1
SET benchmark_name=%2

SCTBenchmarksRunner\bin\Release\net48\SCTBenchmarksRunner.exe -s SCTBenchmarks\bin\Release\Benchmarks.exe -i %iterations% %benchmark_name%

EXIT /B 0

:usage
    echo Usage: %~nx0 iterations BenchmarkName
    EXIT /B 1
