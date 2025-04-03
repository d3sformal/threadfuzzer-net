using System.Threading;

namespace Benchmarks;

[BenchmarkInfo(DegreeOfConcurrency = 5, Notes = "Contains deadlock")]

public class Carter01Bad : IBenchmark
{
    private readonly SemaphoreSlim DataLock1 = new SemaphoreSlim(1,1);
    private readonly SemaphoreSlim DataLock2 = new SemaphoreSlim(1,1);

    private int A;
    private int B;

    public void RunTest()
    {
        var t1 = Utils.Run(() =>
        {
            DataLock1.Wait();
            A++;
            if (A is 1)
            {
                DataLock2.Wait();
            }

            DataLock1.Release();

            // Perform class A operation.
            DataLock1.Wait();
            A--;
            if (A is 0)
            {
                DataLock2.Release();
            }

            DataLock1.Release();
        });

        var t2 = Utils.Run(() =>
        {
            DataLock1.Wait();
            B++;
            if (B is 1)
            {
                DataLock2.Wait();
            }

            DataLock1.Release();

            // Perform class B operation.
            DataLock1.Wait();
            B--;
            if (B is 0)
            {
                DataLock2.Release();
            }

            DataLock1.Release();
        });

        var t3 = Utils.Run(() => {});

        var t4 = Utils.Run(() => {});

        Utils.WaitAll(t1, t2, t3, t4);
    }
}