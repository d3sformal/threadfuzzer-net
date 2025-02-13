# Running (notes)
mklink /D config config_{fuzzing,systematic*}
mkdir traces
run_profiled_benchmark.bat 150 all >results_profiled.txt
run_unprofiled_benchmark.bat 150 all >results_unprofiled.txt

# Running batch (notes)
rm -rf traces_systematic_first traces_systematic_last traces_systematic_random
rm -rf results_systematic_first.txt results_systematic_last.txt results_systematic_random.txt
run_batch_profiled.bat 150 all systematic_first systematic_last systematic_random

