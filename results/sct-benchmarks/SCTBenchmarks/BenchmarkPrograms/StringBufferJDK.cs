using System;
using System.Threading;

namespace Benchmarks
{
    public class StringBufferJDK
    {
        private StringBuffer Buffer = new StringBuffer("abc");

        public void RunTest()
        {
            var task = Utils.Run(() =>
            {
                Buffer.Erase(0, 3);
                Buffer.Append("abc");
            });

            StringBuffer sb = new StringBuffer();
            sb.Append(Buffer);

            Utils.WaitAll(task);
        }

        private class StringBuffer
        {
            private readonly SemaphoreSlim DataLock = new SemaphoreSlim(1,1);

            private char[] Value;
            private int ValueLength;
            private int Count;

            internal StringBuffer()
            {
                Value = new char[16];
                ValueLength = 16;
                Count = 0;
            }

            internal StringBuffer(string str)
                : this(str.ToCharArray())
            {
            }

            internal StringBuffer(char[] str)
            {
                int length = str.Length + 16;
                Value = new char[length];
                ValueLength = length;
                Count = 0;
                Append(str);
            }

            internal StringBuffer Append(string str) => Append(str.ToCharArray());

            internal StringBuffer Append(char[] str)
            {
                DataLock.Wait();
                if (str is null)
                {
                    str = "null".ToCharArray();
                }

                int len = str.Length;
                int newCount = Count + len;
                if (newCount > ValueLength)
                {
                    ExpandCapacity(newCount);
                }

                Array.Copy(str, 0, Value, Count, len);
                Count = newCount;
                DataLock.Release();
                return this;
            }

            internal StringBuffer Append(StringBuffer sb)
            {
                DataLock.Wait();
                if (sb == null)
                {
                    sb = new StringBuffer("null");
                }

                int len = sb.Length();
                int newCount = Count + len;
                if (newCount > ValueLength)
                {
                    ExpandCapacity(newCount);
                }

                sb.GetChars(0, len, Value, Count);
                Count = newCount;
                DataLock.Release();
                return this;
            }

            internal StringBuffer Erase(int start, int end)
            {
                DataLock.Wait();
                Utils.Assert(start >= 0, $"Failed: {start} >= 0");
                if (end > Count)
                {
                    end = Count;
                }

                Utils.Assert(start <= end, $"Failed: {start} <= {end}");
                int len = end - start;
                if (len > 0)
                {
                    Utils.ArrayFill(Value, '\0', start, len);
                    Count -= len;
                }

                DataLock.Release();
                return this;
            }

            private int Length()
            {
                DataLock.Wait();
                int ret = Count;
                DataLock.Release();
                return ret;
            }

            private void GetChars(int srcBegin, int srcEnd, char[] dst, int dstBegin)
            {
                DataLock.Wait();
                Utils.Assert(srcBegin >= 0, $"Failed: {srcBegin} >= 0");
                Utils.Assert(srcEnd >= 0 && srcEnd <= Count, $"Failed: {srcEnd} >= 0 && {srcEnd} <= {Count}");
                Utils.Assert(srcBegin <= srcEnd, $"Failed: {srcBegin} <= {srcEnd}");
                Array.Copy(Value, srcBegin, dst, dstBegin, srcEnd - srcBegin);
                DataLock.Release();
            }

            private void ExpandCapacity(int minimumCapacity)
            {
                int newCapacity = (ValueLength + 1) * 2;
                if (newCapacity < 0)
                {
                    newCapacity = int.MaxValue;
                }
                else if (minimumCapacity > newCapacity)
                {
                    newCapacity = minimumCapacity;
                }

                var newValue = new char[newCapacity];
                Array.Copy(Value, newValue, Count);
                Value = newValue;
                ValueLength = newCapacity;
            }
        }
    }
}
