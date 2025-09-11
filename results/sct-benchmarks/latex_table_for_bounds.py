import os
import pprint

config_suffixes = ["first_bound_5", "first_bound_10", "first_bound_30", "first_bound_100", "random_bound_5", "random_bound_10", "random_bound_30", "random_bound_100"]

data = {}

for cfg in config_suffixes:
    file_name = "results_systematic_" + cfg + ".txt"
    file_in = open(file_name, "r")

    lines = file_in.readlines()
    cl_idx = 0

    while (cl_idx < len(lines)):
        cur_line = lines[cl_idx].strip()

        if cur_line.startswith("Benchmark"):
            benchmark_name = cur_line[10 : cur_line.find(":")]
            
            cl_idx += 1

            iterations_num = -1

            cur_line = lines[cl_idx].strip()
            if cur_line.startswith("Iterations"):
                iterations_num = (cur_line.split(" "))[2]
 
            cl_idx += 2

            violated_pct = -1.0

            cur_line = lines[cl_idx].strip()
            if cur_line.startswith("Violated"):
                violated_pct = cur_line[cur_line.find("(")+1 : cur_line.find(")")-1]
 
            if lines[cl_idx+1].strip().startswith("TimedOut"):
                cl_idx += 1

            cl_idx += 4

            data_for_benchmark = data.get(benchmark_name, {})

            data_for_violated = data_for_benchmark.get("violated", {})
            data_for_violated[cfg] = violated_pct
            data_for_benchmark["violated"] = data_for_violated

            data_for_iterations = data_for_benchmark.get("iterations", {})
            data_for_iterations[cfg] = iterations_num
            data_for_benchmark["iterations"] = data_for_iterations

            data[benchmark_name] = data_for_benchmark

file_out = open("table_bounds.tex", "w")

for bench_name in data:
    data_for_bench = data[bench_name]

    table_row_b1 = "\multirow{2}{*}{ " + bench_name + " } &"

    table_row_b1 += " \multicolumn{2}{l|}{ Violated }"

    data_for_violated = data_for_bench["violated"]

    for cfg in config_suffixes:
        table_row_b1 += " & "
        table_row_b1 += data_for_violated[cfg]
        table_row_b1 += " \%"
    table_row_b1 += " \\\\"

    table_row_b2 = " &"

    table_row_b2 += " \multicolumn{2}{l|}{ Iterations }"

    data_for_iterations = data_for_bench["iterations"]

    for cfg in config_suffixes:
        table_row_b2 += " & "
        table_row_b2 += data_for_iterations[cfg]
    table_row_b2 += " \\\\"

    file_out.write(table_row_b1 + "\n")
    file_out.write(table_row_b2 + "\n")
    file_out.write("\hline\n")

