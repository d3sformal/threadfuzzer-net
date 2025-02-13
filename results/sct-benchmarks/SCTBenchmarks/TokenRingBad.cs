using System.Threading;

namespace Benchmarks
{
    [BenchmarkInfo(DegreeOfConcurrency = 5)]
    public class TokenRingBad : IBenchmark
    {
        private readonly SemaphoreSlim DataLock = new SemaphoreSlim(1,1);

        private int X1 = 1;
        private int X2 = 2;
        private int X3 = 1;

        private bool Flag1;
        private bool Flag2;
        private bool Flag3;

        public void RunTest()
        {
            var t1 = Utils.Run(() =>
            {
                DataLock.Wait();
                X1 = (X3 + 1) % 4;
                Flag1 = true;
                DataLock.Release();
            });

            var t2 = Utils.Run(() =>
            {
                DataLock.Wait();
                X2 = X1;
                Flag2 = true;
                DataLock.Release();
            });

            var t3 = Utils.Run(() =>
            {
                DataLock.Wait();
                X3 = X2;
                Flag3 = true;
                DataLock.Release();
            });

            var t4 = Utils.Run(() =>
            {
                DataLock.Wait();
                if (Flag1 && Flag2 && Flag3)
                {
                    Utils.Assert(X1 == X2 && X2 == X3, "Found bug!");
                }
                DataLock.Release();
            });

            Utils.WaitAll(t1, t2, t3, t4);
        }
    }
}
