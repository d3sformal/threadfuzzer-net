using System;
using System.Threading;

namespace ThreadedHelloWorld;

static class Program
{
    private static int Seconds(this int value) => 1000 * value;

    private static readonly Random Random = new();
    private static void HelloWorldLoop()
    {
        Thread.Sleep(Random.Next(2.Seconds()));

        for (int i = 0; i < 5; ++i)
            F(i);
    }

    private static void F(int i)
    {
        Console.WriteLine($"{Thread.CurrentThread.Name}: Hello World {i}");
    }

    private const int ThreadCount = 2;

    private static void Main()
    {
        Thread[] threads = new Thread[ThreadCount];
        for (int i = 0; i < ThreadCount; ++i)
            threads[i] = new Thread(HelloWorldLoop) {Name = $"Thread {i + 1}"};

        foreach (Thread thread in threads)
            thread.Start();

        foreach (Thread thread in threads)
            thread.Join();
    }
}