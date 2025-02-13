using System;

namespace HelloWorldCore3._1
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("Hello World!");
            Console.WriteLine($"CORECLR_ENABLE_PROFILING={Environment.GetEnvironmentVariable("CORECLR_ENABLE_PROFILING")}");
            Console.WriteLine($"CORECLR_PROFILER={Environment.GetEnvironmentVariable("CORECLR_PROFILER")}");
            Console.WriteLine($"CORECLR_PROFILER_PATH={Environment.GetEnvironmentVariable("CORECLR_PROFILER_PATH")}");
        }
    }
}
