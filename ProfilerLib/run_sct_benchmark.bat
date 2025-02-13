@echo off

setlocal

IF "%~1"=="" GOTO usage;
IF "%~2"=="" GOTO usage;

SET profiler=..\Debug\ProfilerLib.dll
SET config=profiler.conf
SET program=C:\Users\Filip\_Data\Temp\SCTBenchmarks\BenchmarkRunner\bin\Debug\Benchmark.Runner.exe 
SET iterations=%1
SET benchmark_name=%2

cmd /c "FOR /l %%x IN (1, 1, %iterations%) DO @run_profiler.bat %profiler% %config% %program% -i %%x %benchmark_name% & if errorlevel 1 exit /b ^"

EXIT /B 0

:usage
    echo Usage: %~nx0 iterations BenchmarkName
    EXIT /B 1
