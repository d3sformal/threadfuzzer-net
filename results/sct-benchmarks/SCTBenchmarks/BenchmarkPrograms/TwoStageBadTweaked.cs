﻿using System;
using System.Threading;

namespace Benchmarks
{
    [BenchmarkInfo(DegreeOfConcurrency = 99 + 1 + 1, Notes = "fields -> properties")]
    public class TwoStageBadTweaked100: TwoStageBadTweakedBase, IBenchmark
    {
        public void RunTest()
        {
            RunTest(99, 1);
        }
    }

    public class TwoStageBadTweakedBase
    {
        private readonly SemaphoreSlim DataLock1 = new SemaphoreSlim(1,1);
        private readonly SemaphoreSlim DataLock2 = new SemaphoreSlim(1,1);

        private int DataValue1 { get; set; }
        private int DataValue2 { get; set; }

        protected void RunTest(int iTThreads, int iRThreads)
        {
            var tPool = Utils.NewWorkerArray(iTThreads);
            var rPool = Utils.NewWorkerArray(iRThreads);

            for (int i = 0; i < iTThreads; i++) 
            {
                tPool[i] = Utils.Run(() =>
                {
                    DataLock1.Wait();
                    DataValue1 = 1;
                    DataLock1.Release();

                    DataLock2.Wait();
                    DataValue2 = DataValue1 + 1;
                    DataLock2.Release();
                });
            }

            for (int i = 0; i < iRThreads; i++) 
            {
                rPool[i] = Utils.Run(() =>
                {
                    int t1 = -1;
                    int t2 = -1;

                    DataLock1.Wait();
                    if (DataValue1 is 0)
                    {
                        DataLock1.Release();
                        return;
                    }

                    t1 = DataValue1;
                    DataLock1.Release();

                    DataLock2.Wait();
                    t2 = DataValue2;
                    DataLock2.Release();

                    Utils.Assert(t2 == t1 + 1, "Bug found!");
                });
            }

            Utils.WaitAll(tPool);
            Utils.WaitAll(rPool);
        }
    }
}
