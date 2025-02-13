using System.Threading;

namespace Benchmarks
{
    public class WSQ
    {
        private const int NUM_STEALERS = 1;
        private const int NUM_ITEMS = 4;
        private const int NUM_STEAL_ATTEMPTS = 2;

        private WorkStealingQueue Queue = new WorkStealingQueue();
        private ObjType[] Items;

        public void RunTest()
        {
            Items = new ObjType[NUM_ITEMS];
            for (int i = 0; i < NUM_ITEMS; i++)
            {
                Items[i] = new ObjType();
            }

            var stealers = Utils.NewWorkerArray(NUM_STEALERS);
            for (int i = 0; i < NUM_STEALERS; i++)
            {
                stealers[i] = Utils.Run(() =>
                {
                    for(int j = 0; j < NUM_STEAL_ATTEMPTS; j++)
                    {
                        if (Queue.Steal(out ObjType r))
                        {
                            r.Operation();
                        }
                    }
                });
            }

            for (int i = 0; i < NUM_ITEMS / 2; i++)
            {
                Queue.Push(Items[2 * i]);
                Queue.Push(Items[2 * i + 1]);
                if (Queue.Pop(out ObjType r))
                {
                    r.Operation();
                }
            }

            for (int i = 0; i < NUM_ITEMS / 2; i++)
            {
                if (Queue.Pop(out ObjType r))
                {
                    r.Operation();
                }
            }

            Utils.WaitAll(stealers);

            for (int i = 0; i < NUM_ITEMS; i++)
            {
                Items[i].Check();
            }
        }

        private class ObjType
        {
            private int Field;

            internal ObjType()
            {
                Field = 0;
            }

            internal void Operation()
            {
                Field++;
            }

            internal void Check()
            {
                Utils.Assert(Field is 1, "Bug found!");
            }
        }

        private class WorkStealingQueue
        {
            private const int INITQSIZE = 2; // Must be power of 2.
            private const long InitialSize = 1024; // Must be a power of 2.
            private const long MaxSize = 1024 * 1024;

            private readonly SemaphoreSlim DataLock = new SemaphoreSlim(1,1);

            private ObjType[] Elems;
            private long Head { get; set; }
            private long Tail { get; set; }
            private long Mask { get; set; }

            internal WorkStealingQueue()
            {
                Head = 0;
                Tail = 0;
                Mask = INITQSIZE - 1;
                Elems = new ObjType[INITQSIZE];
            }

            internal bool Steal(out ObjType result)
            {
                bool found;
                DataLock.Wait();

                long h = Head;
                Head = h + 1;

                if (h < Tail)
                {
                    result = Elems[h & Mask];
                    found = true;
                }
                else
                {
                    Head = h;
                    result = null;
                    found = false;
                }

                DataLock.Release();
                return found;
            }

            internal void Push(ObjType elem)
            {
                long t = Tail;
                if (t < Head + Mask + 1 && t < MaxSize)
                {
                    Elems[t & Mask] = elem;
                    Tail = t + 1;
                }
                else
                {
                    SyncPush(elem);
                }
            }

            private void SyncPush(ObjType elem)
            {
                DataLock.Wait();

                long h = Head;
                long count = Tail - h;

                h = h & Mask;
                Head = h;
                Tail = h + count;

                if (count >= Mask)
                {
                    long newsize = (Mask == 0 ? InitialSize : 2 * (Mask + 1));

                    Utils.Assert(newsize < MaxSize, $"Failed: {newsize} < {MaxSize}");

                    ObjType[] newtasks = new ObjType[newsize];
                    for (long i = 0; i < count; i++)
                    {
                        newtasks[i] = Elems[(h + i) & Mask];
                    }

                    Elems = newtasks;
                    Mask = newsize - 1;
                    Head = 0;
                    Tail = count;
                }

                Utils.Assert(count < Mask, $"Failed: {count} < {Mask}");

                long t = Tail;
                Elems[t & Mask] = elem;
                Tail = t + 1;
                DataLock.Release();
            }

            internal bool Pop(out ObjType result)
            {
                long t = Tail - 1;
                Tail = t;

                if (Head <= t)
                {
                    result = Elems[t & Mask];
                    return true;
                }

                Tail = t + 1;
                return SyncPop(out result);
            }

            private bool SyncPop(out ObjType result)
            {
                bool found;
                DataLock.Wait();

                long t = Tail - 1;
                Tail = t;
                if (Head <= t)
                {
                    result = Elems[t & Mask];
                    found = true;
                }
                else
                {
                    Tail = t + 1;
                    result = null;
                    found = false;
                }

                if (Head > t)
                {
                    Head = 0;
                    Tail = 0;
                    found = false;
                }

                DataLock.Release();
                return found;
            }
        }
    }
}
