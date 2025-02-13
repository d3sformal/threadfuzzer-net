using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using CommandLineParser.Arguments;
using CommandLineParser.Exceptions;
using OptionsParser = CommandLineParser.CommandLineParser;

namespace SCTBenchmarksRunner;

internal class ParsedOptions
{
    [ValueArgument(typeof(string), 'p', "profiler", Description = "DLL of the profiler")]
    public string ProfilerDll { get; set; }

    [BoundedValueArgument(typeof(int), 'i', "iterations", DefaultValue = 1, MinValue = 1, MaxValue = int.MaxValue, Description = "Number of iterations")]
    public int Iterations { get; set; }

    [SwitchArgument('l', "list", false, Description = "Lists available benchmarks")]
    public bool ListBenchmarks { get; set; }

    [BoundedValueArgument(typeof(int), 't', "timeout", MinValue = 0, MaxValue = 3600, DefaultValue = 30, Description = "Timeout (in seconds)")]
    public int Timeout { get; set; }

    [ValueArgument(typeof(string), 's', "subject", Description = "Assembly containing benchmarks")]
    public string SubjectProgram { get; set; }

    public List<string> BenchmarkSpecs { get; } = new();

    public static ParsedOptions ParseOptions(string[] args)
    {
        var options = new ParsedOptions();
        var parser = new OptionsParser
        {
            ShowUsageHeader = $"Usage: {Assembly.GetExecutingAssembly().GetName().Name} [options] most | all | BenchmarkSpec [...]",
            ShowUsageOnEmptyCommandline = true,
            ShowUsageCommands = { "-h" }
        };
        parser.ExtractArgumentAttributes(options);
        try
        {
            parser.ParseCommandLine(args);
            if (!parser.ParsingSucceeded)
                return null;
        }
        catch (CommandLineException ex)
        {
            Console.Error.WriteLine(ex);
            parser.ShowUsage();
            return null;
        }

        options.BenchmarkSpecs.AddRange(parser.AdditionalArgumentsSettings.AdditionalArguments);
        return options;
    }
}

internal class Program
{
    private static bool RunBenchmark(Process process, ParsedOptions options, string benchmarkName)
    {
        process.StartInfo.RedirectStandardOutput = true;
        process.StartInfo.UseShellExecute = false;

        process.StartInfo.FileName = options.SubjectProgram;
        process.StartInfo.Arguments = $"{benchmarkName}";

        if (options.ProfilerDll != null)
        {
            process.StartInfo.EnvironmentVariables["COR_ENABLE_PROFILING"] = "1";
            process.StartInfo.EnvironmentVariables["COR_PROFILER"] = "{00000000-0000-0000-0000-000000000000}";
            process.StartInfo.EnvironmentVariables["COR_PROFILER_PATH"] = options.ProfilerDll;

            process.StartInfo.EnvironmentVariables["COR_PROFILER_CONFIG_FILE"] = $"./config/{benchmarkName}.conf";
        }

        process.Start();
        if (options.Timeout > 0)
            return process.WaitForExit(options.Timeout * 1000);
        process.WaitForExit();
        return true;
    }

    private struct BenchmarkRunStats
    {
        public int Iterations = 0;
        public int Passed = 0;
        public int AssertionViolated = 0;
        public int TimedOut = 0;
        public long TimeTaken = 0;

        public BenchmarkRunStats()
        {
        }
    }

    private static string PassedMessage { get; set; }
    private static string AssertionViolatedMessage { get; set; }

    private static void ProcessBenchmarkRun(BenchmarkInfo benchmark, ParsedOptions options)
    {
        var stats = new BenchmarkRunStats();

        Console.Write($"Benchmark {benchmark.Name}: ");
        for (int i = 1; i <= options.Iterations; ++i)
        {
            stats.Iterations++;

            using var process = new Process();
            var sw = Stopwatch.StartNew();
            var timedOut = !RunBenchmark(process, options, benchmark.Name);
            sw.Stop();

            if (timedOut)
            {
                stats.TimedOut++;
                process.Kill();
            }
            else
            {
                for (string line; (line = process.StandardOutput.ReadLine()) != null;)
                {
                    if (line.Contains(PassedMessage))
                        stats.Passed++;

                    else if (line.Contains(AssertionViolatedMessage))
                        stats.AssertionViolated++;
                }
                stats.TimeTaken += sw.ElapsedMilliseconds;
            }
            Console.Write(".");
        }

        Console.WriteLine();
        Console.WriteLine($"  Iterations = {stats.Iterations}");
        Console.WriteLine($"      Passed = {stats.Passed} ({100.0 * stats.Passed / stats.Iterations:F1}%)");
        Console.WriteLine($"    Violated = {stats.AssertionViolated} ({100.0 * stats.AssertionViolated / stats.Iterations:F1}%)");
        if (stats.TimedOut > 0)
            Console.WriteLine($"    TimedOut = {stats.TimedOut} ({100.0 * stats.TimedOut / stats.Iterations:F1}%)");
        Console.WriteLine($"  TimePerRun = {1.0 * stats.TimeTaken / (stats.Iterations - stats.TimedOut):F1} ms");
        Console.WriteLine($"   DegreeOfC = {benchmark.DegreeOfConcurrency}");
        Console.WriteLine($"        SLOC = {benchmark.LinesOfCode} ({benchmark.SizeInBytes / 1024.0:F1} kB)");
    }

    internal readonly struct BenchmarkInfo
    {
        private string FilePath { get; }

        public int DegreeOfConcurrency { get; }

        public string Name { get; }
        public int LinesOfCode => GetLineCount(FilePath);
        public long SizeInBytes => GetFileSize(FilePath);

        public override string ToString()
        {
            return Name;
        }

        public BenchmarkInfo(string name, string filePath, int degreeOfConcurrency)
        {
            Name = name;
            FilePath = filePath;
            DegreeOfConcurrency = degreeOfConcurrency;
        }
        
        private static long GetFileSize(string filePath)
        {
            return new FileInfo(filePath).Length;
        }

        private static int GetLineCount(string filePath)
        {
            return File.ReadLines(filePath).Count();
        }
    }

    internal static readonly List<BenchmarkInfo> AllBenchmarks = new();

    private static void InspectBenchmarksAssembly(string assemblyPath)
    {
        static T GetValueOfProperty<T>(object obj, string propertyName)
        {
            var value = obj.GetType().GetProperty(propertyName)?.GetValue(obj);
            if (value is T tValue)
                return tValue;
            throw new ArgumentException($"Type {obj!.GetType().Name} doesn't have property {propertyName} set.");
        }

        static string GetStringValueOfStaticField(Type type, string fieldName)
        {
            return type?.GetField(fieldName, BindingFlags.Public | BindingFlags.Static)?.GetValue(null) as string ?? throw new ArgumentException($"Type {type!.Name} doesn't have static field {fieldName} set.");
        }
        
        var assembly = Assembly.UnsafeLoadFrom(assemblyPath);
        var program = assembly.GetType("Benchmarks.Program");
        PassedMessage = GetStringValueOfStaticField(program, nameof(PassedMessage));
        AssertionViolatedMessage = GetStringValueOfStaticField(program, nameof(AssertionViolatedMessage));

        var benchmarkTypes = assembly.GetTypes().Where(t => t.GetInterface("IBenchmark") != null);

        foreach (var benchmarkType in benchmarkTypes)
        {
            var attribute = benchmarkType.GetCustomAttributes().FirstOrDefault(attr => attr.GetType().Name == "BenchmarkInfoAttribute");
            var benchmarkInfo = new BenchmarkInfo(benchmarkType.Name, GetValueOfProperty<string>(attribute, "FilePath"), GetValueOfProperty<int>(attribute, "DegreeOfConcurrency"));
            AllBenchmarks.Add(benchmarkInfo);
        }
    }

    static void Main(string[] args)
    {
        var options = ParsedOptions.ParseOptions(args);
        if (options == null)
            return;

        InspectBenchmarksAssembly(options.SubjectProgram);


        if (options.ListBenchmarks)
        {
            Console.WriteLine(string.Join(", ", AllBenchmarks));
            return;
        }

        IEnumerable<BenchmarkInfo> benchmarksToRun = null;

        if (options.BenchmarkSpecs.Count == 1)
        {
            benchmarksToRun = options.BenchmarkSpecs[0].ToLower() switch
            {
                "most" => AllBenchmarks.Where(b => b.Name != "Carter01Bad" && b.Name != "Deadlock01Bad" && b.Name != "TwoStageBadTweaked100"),
                "all" => AllBenchmarks,
                _ => null
            };
        }

        benchmarksToRun ??= AllBenchmarks.Where(t => options.BenchmarkSpecs.Any(ot => t.ToString() == ot));

        bool hadRun = false;

        foreach (var benchmark in benchmarksToRun)
        {
            ProcessBenchmarkRun(benchmark, options);
            hadRun = true;
        }

        if (!hadRun)
        {
            Console.WriteLine($"'{string.Join(", ", options.BenchmarkSpecs)}' doesn't match any test");
        }
    }
}
