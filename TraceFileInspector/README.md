# Running (graph mode)
..\Release\TraceFileInspector.exe file.trace --graph >tex-graph\graph.tex.inc
latexmk -pdf -cd tex-graph

# Running (normal mode)
Prints information about individual traces (i.e. parses the binary trace file format)
