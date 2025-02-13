using System;
using System.Collections.Generic;
using System.Threading;

namespace ProducerConsumerQueue;

internal static class Program
{
    private static readonly Queue<int> Queue = new Queue<int>();
    private const int IterationCount = 50;
    private const int QueueSize = 5;
    private const int SleepTimer = 500;

    private static void Producer()
    {
        Random r = new Random(42);

        for (int i = 0; i < IterationCount; ++i)
        {
            lock (Queue)
            {
                while (Queue.Count == QueueSize)
                    Monitor.Wait(Queue);

                Console.WriteLine($"En-queuing: {i}");
                Queue.Enqueue(i);

                Monitor.Pulse(Queue);
            }

            Thread.Sleep(r.Next(SleepTimer));
        }
    }

    private static void Consumer()
    {
        Random r = new Random(43);

        for (int i = 0; i < IterationCount; ++i)
        {
            lock (Queue)
            {
                while (Queue.Count == 0)
                    Monitor.Wait(Queue);


                Console.WriteLine($"De-queuing: {Queue.Dequeue()}");

                Monitor.Pulse(Queue);
            }

            Thread.Sleep(r.Next(SleepTimer));
        }
    }

    public static void Main(string[] args)
    {
        Thread producer = new Thread(Producer);
        Thread consumer = new Thread(Consumer);

        producer.Start();
        consumer.Start();

        producer.Join();
        consumer.Join();
    }
}