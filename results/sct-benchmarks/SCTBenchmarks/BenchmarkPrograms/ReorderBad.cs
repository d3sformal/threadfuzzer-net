﻿using System.Threading;

namespace Benchmarks
{
    [BenchmarkInfo(DegreeOfConcurrency = 4 + 12 + 1)]

    public class ReorderBad3 : ReorderBadBase, IBenchmark
    {
        public void RunTest()
        {
            RunTest(4, 12);
        }
    }

    [BenchmarkInfo(DegreeOfConcurrency = 5 + 16 + 1)]

    public class ReorderBad4 : ReorderBadBase, IBenchmark
    {
        public void RunTest()
        {
            RunTest(5, 16);
        }
    }

    [BenchmarkInfo(DegreeOfConcurrency = 6 + 20 + 1)]
    public class ReorderBad5 : ReorderBadBase, IBenchmark
    {
        public void RunTest()
        {
            RunTest(6, 20);
        }
    }

    [BenchmarkInfo(DegreeOfConcurrency = 11 + 40 + 1)]
    public class ReorderBad10 : ReorderBadBase, IBenchmark
    {
        public void RunTest()
        {
            RunTest(11, 40);
        }
    }

    [BenchmarkInfo(DegreeOfConcurrency = 21 + 89 + 1)]
    public class ReorderBad20 : ReorderBadBase, IBenchmark
    {
        public void RunTest()
        {
            RunTest(21, 89);
        }
    }

    public class ReorderBadBase
    {
        private int A = 0;
        private int B = 0;

        protected void RunTest(int iSet, int iCheck)
        {
            var setPool = Utils.NewWorkerArray(iSet);
            var checkPool = Utils.NewWorkerArray(iCheck);

            for (int i = 0; i < iSet; i++) 
            {
                setPool[i] = Utils.Run(() =>
                {
                    this.A = 1;
                    this.B = -1;
                });
            }

            for (int i = 0; i < iCheck; i++) 
            {
                checkPool[i] = Utils.Run(() =>
                {
                    if (!((this.A is 0 && this.B is 0) || (this.A is 1 && this.B is -1)))
                    {
                        Utils.Assert(false, "Bug found!");
                    }
                });
            }

            Utils.WaitAll(setPool);
            Utils.WaitAll(checkPool);
        }
    }
}
