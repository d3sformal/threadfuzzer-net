using System;
using System.Threading;

namespace Benchmarks
{
    [BenchmarkInfo(DegreeOfConcurrency = 4)]

    public class Lazy01Bad : IBenchmark
    {
        private readonly SemaphoreSlim DataLock = new SemaphoreSlim(1, 1);

        private int Data;

        public void RunTest()
        {
            var t1 = Utils.Run(() =>
            {
                DataLock.Wait();
                Data++;
                DataLock.Release();
            });

            var t2 = Utils.Run(() =>
            {
                DataLock.Wait();
                Data += 2;
                DataLock.Release();
            });

            var t3 = Utils.Run(() =>
            {
                DataLock.Wait();
                Utils.Assert(Data is 3, "Bug found!");
                DataLock.Release();
            });

            Utils.WaitAll(t1, t2, t3);
        }
    }
}
