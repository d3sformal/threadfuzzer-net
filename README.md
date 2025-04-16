This project contains the prototype implementation of a fuzzing tool for multithreaded C#/.NET programs on Windows systems, and few small benchmarks.
Developed by Filip Kliber and Pavel Parízek at Department of Distributed and Dependable Systems, Charles University, Prague, Czech Republic.

More information about the tool, including overall architecture, technical details, and fuzzing supported algorithms, is available in the paper "Locating Concurrency Errors in Windows .NET Applications by Fuzzing over Thread Schedules", to be published at SPIN 2025.

The tool was tested on [Windows11 Evaluation](https://www.microsoft.com/en-us/evalcenter/download-windows-11-enterprise) using [Visual Studio 2022 Community](https://visualstudio.microsoft.com/vs/community/) (with installed C++ and .NET Framework development support). Currently, only `x86` variant is supported and all projects need to be compiled as such. Please refer to official guide for [Visual Studio solution configuration](https://learn.microsoft.com/en-us/visualstudio/ide/understanding-build-configurations?view=vs-2022).

## Repository layout
The following diagram points to relevant folders and files in this repository

```
threadfuzzer-net/
├─ ConsoleProvider/         (Project used for console-driver)
├─ ProfilerLib/             (Implementation of the tool, builds into ProfilerLib.dll)
│  ├─ src/                    (Source code of the profiler)
│  ├─ profiler.conf.sample    (Sample configuration with description of individual settings)
│  ├─ run_profiler.bat        (BAT script to run arbitrary NET4.8 app with the profiler attached)
├─ TraceFileInspector/      (Used to inspect domain specific trace file format)
├─ TraceFileStats/          (Used to parse domain specific trace file format and produce stats)
├─ results/                 (Framework for running the profiler on benchmarks and collecting results)
│  ├─ sct-benchmarks/         (SCTBenchmarks; has its own README.md)
│  ├─ test-projects/          (Test projects used during development)
├─ ThreadFuzzer.NET.sln     (Main solution file for compiling everything)
├─ README.md                (This file)
```

## Installation
Currently, there is no system-wide installation. Provided solution file builds the library into `./Release/ProfilerLib.dll` (or `./Debug/ProfilerLib.dll`). The step-by-step process would be

- Open `ThreadFuzzer.NET.sln`
- Change configuration to `Release`, `x86` in menu
- Build -> Build solution

## Running on sample program
To allow CLR to attach the profiler to the application, several environmental variables need to be set. The provided `run_profiler.bat` script does exactly that and can be invoked in the following way:

```
ProfilerLib\run_profiler.bat Release\ProfilerLib.dll <path to configuration> <path to application executable>
```

For example, to run it on provided HelloWorld project, that also contains the sample configuration, the command would be

```
ProfilerLib\run_profiler.bat Release\ProfilerLib.dll results\test-projects\HelloWorld\profiler.conf results\test-projects\HelloWorld\bin\Release\HelloWorld.exe
```

If everything works, the standard output of the hello world should be interleaved with information from the profiler and can look like this:

```
[      2913] [23108] [Info] [ThreadCreated] 13915688
[      4751] [23108] [Info] [ThreadAssignedToOSThread] 13915688 -> 23108
[      7661] [ 9144] [Info] [ThreadCreated] 13969672
[      9619] [ 9144] [Info] [ThreadAssignedToOSThread] 13969672 -> 9144
[     69084] [23108] [Info] Enabled thread_control: Good Luck [id: 11128]
Hello World!
COR_ENABLE_PROFILING=1
COR_PROFILER={00000000-0000-0000-0000-000000000000}
COR_PROFILER_PATH=C:\...\threadfuzzer-net\Release\ProfilerLib.dll
[    103246] [23108] [Info] Disabled thread_control
[    135118] [23108] [Info] Profiler exiting.
```
