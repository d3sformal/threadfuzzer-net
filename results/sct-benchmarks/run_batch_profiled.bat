@echo off

IF "%~1"=="" GOTO usage;
IF "%~2"=="" GOTO usage;

set allargs=%*
CALL SET rest=%%allargs:*%2=%%

SET iterations=%1
SET benchmark_name=%2

for %%x in (%rest%) do (
    mkdir traces
    mklink /D config config_%%x

    run_profiled_benchmark.bat %iterations% %benchmark_name% > results_%%x.txt
    mv traces traces_%%x
    rm config
)

EXIT /B 0

:usage
    echo Usage: %~nx0 iterations BenchmarkName
    EXIT /B 1
