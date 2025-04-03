using System.Threading;

namespace Benchmarks
{
    [BenchmarkInfo(DegreeOfConcurrency = 4)]
    public class AccountBad : IBenchmark
    {
        private readonly SemaphoreSlim DataLock = new SemaphoreSlim(1, 1);

        private int X = 1;
        private int Y = 2;
        private int Z = 4;
        private int Balance;
        private bool DepositDone;
        private bool WithdrawDone;

        public void RunTest()
        {
            Balance = X;

            var t1 = Utils.Run(() =>
            {
                DataLock.Wait();
                if (DepositDone && WithdrawDone)
                {
                    Utils.Assert(Balance == X - Y - Z, "Bug found!");
                }

                DataLock.Release();
            });

            var t2 = Utils.Run(() =>
            {
                DataLock.Wait();
                Balance += Y;
                DepositDone = true;
                DataLock.Release();
            });

            var t3 = Utils.Run(() =>
            {
                DataLock.Wait();
                Balance -= Z;
                WithdrawDone = true;
                DataLock.Release();
            });

            Utils.WaitAll(t1, t2, t3);
        }
    }
}