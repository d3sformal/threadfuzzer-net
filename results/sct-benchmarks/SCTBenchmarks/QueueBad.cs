using System.Threading;

namespace Benchmarks
{
    [BenchmarkInfo(DegreeOfConcurrency = 3)]

    public class QueueBad : IBenchmark
    {
        private const int SIZE = 20;
        private const int EMPTY = -1;
        private const int FULL = -2;

        private readonly SemaphoreSlim DataLock = new SemaphoreSlim(1,1);

        private readonly int[] StoredElements = new int[SIZE];
        private readonly int[] Elements = new int[SIZE];
        private int Head;
        private int Tail;
        private int Amount;
        private bool EnqueueFlag = true;
        private bool DequeueFlag;

        public void RunTest()
        {
            var t1 = Utils.Run(() =>
            {
                int value = 0;
                DataLock.Wait();
                StoredElements[0] = value;
                DataLock.Release();

                for (int i = 0; i < (SIZE - 1); i++)
                {
                    DataLock.Wait();
                    if (EnqueueFlag)
                    {
                        value++;
                        Enqueue(value);
                        StoredElements[i+1] = value;
                        EnqueueFlag = false;
                        DequeueFlag = true;
                    }
                    DataLock.Release();
                }
            });

            var t2 = Utils.Run(() =>
            {
                for (int i = 0; i < SIZE; i++)
                {
                    DataLock.Wait();
                    if (DequeueFlag)
                    {
                        int dequeued = Dequeue();
                        int expected = StoredElements[i];
                        Utils.Assert(dequeued == expected, $"Dequeued element '{dequeued}' is not the expected '{expected}'.");
                        DequeueFlag = false;
                        EnqueueFlag = true;
                    }
                    DataLock.Release();
                }
            });

            Utils.WaitAll(t1, t2);
        }

        internal void Enqueue(int x)
        {
            int tail = Tail;
            Elements[Tail] = x;
            Amount++;
            if (Tail == SIZE)
            {
                Tail = 1;
            }
            else
            {
                Tail++;
            }
        }

        internal int Dequeue()
        {
            int head = Head;
            int x = Elements[head];
            Amount--;
            if (Head == SIZE)
            {
                Head = 1;
            }
            else
            {
                Head++;
            }

            return x;
        }
    }
}
