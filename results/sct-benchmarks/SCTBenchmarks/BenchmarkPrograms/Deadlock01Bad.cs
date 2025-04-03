using System.Threading;

namespace Benchmarks
{
    [BenchmarkInfo(DegreeOfConcurrency = 3, Notes = "Contains deadlock")]
    public class Deadlock01Bad : IBenchmark
    {
        private readonly SemaphoreSlim DataLock1 = new SemaphoreSlim(1,1);
        private readonly SemaphoreSlim DataLock2 = new SemaphoreSlim(1,1);

        private int Counter = 1;

        public static void Test()
        {
            Deadlock01Bad test = new Deadlock01Bad();
            test.RunTest();
        }

        public void RunTest()
        {
            var t1 = Utils.Run(() =>
            {
                DataLock1.Wait();
                DataLock2.Wait();
                Counter++;
                DataLock2.Release();
                DataLock1.Release();
            });

            var t2 = Utils.Run(() =>
            {
                DataLock2.Wait();
                DataLock1.Wait();
                Counter--;
                DataLock1.Release();
                DataLock2.Release();
            });

            Utils.WaitAll(t1, t2);
        }
    }
}
