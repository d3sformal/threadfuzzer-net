# Running single configuration
Make sure that both SCTBenchmarks and SCTBenchmarksRunner solutions are built

First, we symlink a config directory (this might require elevated permissions) and prepare directory where traces will be stored:

    mklink /D config config_{fuzzing,systematic*}
    mkdir traces

Then we run the benchmarks

    run_profiled_benchmark.bat 150 all >results_profiled.txt
    run_unprofiled_benchmark.bat 150 all >results_unprofiled.txt

Note that this will take hours to run.

`results_profiled.txt` contain individual sections for every benchmark. Each section contains

- Amount of iterations
- How many iterations Passed/Failed/TimedOut
- Average TimePerRun
- Degree of Concurrency and Size of the benchmark (SLOC)

On top of that, `traces` directory contains traces for each benchmark (multiple iterations are collected in single file). These can be further inspected via provided tools (`TraceFileInspector` and `TraceFileStats`).

# Running multiple configurations in batch
We clear the old traces and results

    rm -rf traces_systematic_first traces_systematic_last traces_systematic_random
    rm -rf results_systematic_first.txt results_systematic_last.txt results_systematic_random.txt

And run everything

    run_batch_profiled.bat 150 all systematic_first systematic_last systematic_random
