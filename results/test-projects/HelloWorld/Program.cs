using System;

namespace HelloWorld
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("Hello World!");
            Console.WriteLine($"COR_ENABLE_PROFILING={Environment.GetEnvironmentVariable("COR_ENABLE_PROFILING")}");
            Console.WriteLine($"COR_PROFILER={Environment.GetEnvironmentVariable("COR_PROFILER")}");
            Console.WriteLine($"COR_PROFILER_PATH={Environment.GetEnvironmentVariable("COR_PROFILER_PATH")}");
        }
    }
}
