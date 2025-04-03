using System.Threading;

namespace Benchmarks
{
    [BenchmarkInfo(DegreeOfConcurrency = 2)]
    public class BluetoothDriverBad : IBenchmark
    {
        private readonly SemaphoreSlim DataLock = new SemaphoreSlim(1,1);

        private int PendingIo = 1;
        private bool StoppingFlag;
        private bool StoppingEvent;
        private bool Stopped;

        public void RunTest()
        {
            var task = Utils.Run(BCSP_PnpStop);
            BCSP_PnpAdd();
            Utils.WaitAll(task);
        }

        private void BCSP_PnpAdd()
        {
            int status = BCSP_IoIncrement();
            if (status is 0)
            {
                // Do work here.
                Utils.Assert(!Stopped, "Bug found!");
            }

            BCSP_IoDecrement();
        }

        private int BCSP_IoIncrement()
        {
            if (StoppingFlag)
            {
                return -1;
            }

            DataLock.Wait();
            PendingIo += 1;
            DataLock.Release();

            return 0;
        }

        private void BCSP_IoDecrement()
        {
            int pendingIo;

            DataLock.Wait();
            PendingIo -= 1;
            pendingIo = PendingIo;
            DataLock.Release();

            if (pendingIo is 0)
            {
                StoppingEvent = true;
            }
        }

        private void BCSP_PnpStop()
        {
            StoppingFlag = true;
            BCSP_IoDecrement();
            if(StoppingEvent)
            {
                // Release allocated resource.
                Stopped = true;
            }
        }
    }
}