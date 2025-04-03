using System.Threading;

namespace Benchmarks
{
    [BenchmarkInfo(DegreeOfConcurrency = 3, Notes = "higher timeout")]
    public class StackBad : IBenchmark
    {
        private const int SIZE = 10;
        private const int OVERFLOW = -1;
        private const int UNDERFLOW = -2;

        private readonly SemaphoreSlim DataLock = new SemaphoreSlim(1, 1);

        private int Top;
        private int[] Arr = new int[SIZE];
        private bool Flag;

        public void RunTest()
        {
            var t1 = Utils.Run(() =>
            {
                for(int i = 0; i < SIZE; i++)
                {
                    DataLock.Wait();
                    Utils.Assert(Push(i) != OVERFLOW, "Failed: push(arr,i)!=OVERFLOW");
                    Flag = true;
                    DataLock.Release();
                }
            });

            var t2 = Utils.Run(() =>
            {
                for(int i = 0; i < SIZE; i++)
                {
                    DataLock.Wait();
                    if (Flag)
                    {
                        Utils.Assert(Pop() != UNDERFLOW, "Bug found!");
                    }

                    DataLock.Release();
                }
            });


            Utils.WaitAll(t1, t2);
        }

        private int Push(int x)
        {
            if (Top is SIZE)
            {
                return OVERFLOW;
            }

            Arr[Top] = x;
            Top++;

            return 0;
        }

        private int Pop()
        {
            if (Top is 0)
            {
                return UNDERFLOW;
            } 

            Top--;
            return Arr[Top];
        }
    }
}
