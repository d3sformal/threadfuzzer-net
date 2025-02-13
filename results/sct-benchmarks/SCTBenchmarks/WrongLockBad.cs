using System.Threading;

namespace Benchmarks
{
    [BenchmarkInfo(DegreeOfConcurrency = 1 + 7 + 1)]
    public class WrongLockBad : WrongLockBadBase, IBenchmark
    {
        public void RunTest()
        {
            RunTest(1, 7);
        }
    }

    [BenchmarkInfo(DegreeOfConcurrency = 1 + 3 + 1)]
    public class WrongLockBad3 : WrongLockBadBase, IBenchmark
    {
        public void RunTest()
        {
            RunTest(1, 3);
        }
    }

    public class WrongLockBadBase
    {
        private readonly SemaphoreSlim DataLock1 = new SemaphoreSlim(1,1);
        private readonly SemaphoreSlim DataLock2 = new SemaphoreSlim(1,1);

        private int DataValue;

        protected void RunTest(int iNum1, int iNum2)
        {
            var num1Pool = Utils.NewWorkerArray(iNum1);
            var num2Pool = Utils.NewWorkerArray(iNum2);

            for (int i = 0; i < iNum1; i++)
            {
                num1Pool[i] = Utils.Run(() =>
                {
                    DataLock1.Wait();
                    int x = DataValue;
                    DataValue++;
                    Utils.Assert(DataValue == x + 1, "Bug found!");
                    DataLock1.Release();
                });
            }

            for (int i = 0; i < iNum2; i++)
            {
                num2Pool[i] = Utils.Run(() =>
                {
                    DataLock2.Wait();
                    DataValue++;
                    DataLock2.Release();
                });
            }

            Utils.WaitAll(num1Pool);
            Utils.WaitAll(num2Pool);
        }
    }
}
