// #define USE_RAW_THREADS

using System;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

[assembly: InternalsVisibleTo("Benchmark.Runner")]

namespace Benchmarks
{
    public class AssertionViolationException : Exception
    {
        public AssertionViolationException(string message) : base(message)
        {
        }
    }

    public sealed class BenchmarkInfoAttribute : Attribute
    {
        public int DegreeOfConcurrency { get; set; }
        public string Notes { get; set; }

        public string FilePath { get; }
        public BenchmarkInfoAttribute([CallerFilePath] string file = "")
        {
            FilePath = file;
        }
    }

    internal static class Utils
    {
        public static string GetCurrentFileName([CallerFilePath] string filePath = "")
        {
            return filePath;
        }

        public const bool IsRawThreads =
#if USE_RAW_THREADS
            true;
#else
            false;
#endif

        public static void Assert(bool condition, string message)
        {
            if (!condition)
                throw new AssertionViolationException(message);
        }

        public static void ArrayFill<T>(T[] array, T element, int begin, int length)
        {
            for (int i = begin; i < begin + length; ++i)
                array[i] = element;
        }

#if USE_RAW_THREADS
        public static Thread Run(ThreadStart action)
        {
            var t = new Thread(action);
            t.Start();
            return t;
        }

        public static void WaitAll(params Thread[] workers)
        {
            foreach (var worker in workers)
                worker.Join();
        }

        public static Thread[] NewWorkerArray(int count)
        {
            return new Thread[count];
        }
#else
        public static Task Run(Action action)
        {
            return Task.Run(action);
        }

        public static void WaitAll(params Task[] workers)
        {
            while (!Task.WaitAll(workers, 500))
            {
                var faulted = workers.FirstOrDefault(w => w.IsFaulted);
                if (faulted != null)
                    throw faulted.Exception.InnerException;
            }
        }

        public static Task[] NewWorkerArray(int count)
        {
            return new Task[count];
        }
#endif
    }

    public interface IBenchmark
    {
        void RunTest();
    }
}
