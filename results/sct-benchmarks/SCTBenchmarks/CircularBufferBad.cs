using System.Threading;

namespace Benchmarks
{
    [BenchmarkInfo(DegreeOfConcurrency = 3)]
    public class CircularBufferBad : IBenchmark
    {
        private const int BUFFER_MAX = 10;
        private const int N = 7;
        private const int ERROR = -1;

        private readonly SemaphoreSlim DataLock = new SemaphoreSlim(1,1);

        private int[] Buffer = new int[BUFFER_MAX];
        private uint First;
        private uint Next;
        private int BufferSize = BUFFER_MAX;
        private bool Send = true;
        private bool Receive;

        public void RunTest()
        {
            var t1 = Utils.Run(() =>
            {
                for (int i = 0; i < N; i++)
                {
                    DataLock.Wait();
                    if (Send) 
                    {
                        InsertLogElement(i);
                        Send = false;
                        Receive = true;
                    }

                    DataLock.Release();
                }  
            });

            var t2 = Utils.Run(() =>
            {
                for (int i = 0; i < N; i++)
                {
                    DataLock.Wait();
                    if (Receive) 
                    {
                        Utils.Assert(RemoveLogElement() == i, "Bug found!");
                        Receive = false;
                        Send = true;
                    }

                    DataLock.Release();
                }
            });

            Utils.WaitAll(t1, t2);
        }

        private int InsertLogElement(int b) 
        {
            if (Next < BufferSize && BufferSize > 0) 
            {
                Buffer[Next] = b;
                Next = (uint)((Next + 1) % BufferSize);
                Utils.Assert(Next < BufferSize, "Next is not less than buffer size.");
            } 
            else 
            {
                return ERROR;
            }

            return b;
        }

        private int RemoveLogElement() 
        {
            Utils.Assert(First >= 0, "First is not positive.");
            if (Next > 0 && First < BufferSize) 
            {
                First++;
                return Buffer[First - 1];
            }

            return ERROR;
        }
    }
}
