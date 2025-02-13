using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Threading;

namespace MSTestTests;

class BankAccount
{
    public static int Balance { get; private set; } = 0;

    public static void Process()
    {
        Thread producer = new Thread(() => Balance += 100);
        Thread consumer = new Thread(() => Balance -= 100);

        producer.Start();
        consumer.Start();

        producer.Join();
        consumer.Join();
    }
}

[TestClass]
public class BankAccountTests
{
    [TestMethod]
    public void TestProcess()
    {
        BankAccount.Process();
        Assert.AreEqual(0, BankAccount.Balance);
    }
}