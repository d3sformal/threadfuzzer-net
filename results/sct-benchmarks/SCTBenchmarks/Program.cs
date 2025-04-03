using System;

namespace Benchmarks;

public static class Program
{
    public static string PassedMessage = "Passed.";
    public static string AssertionViolatedMessage = "Assertion violation.";

    static void Main(string[] args)
    {
        if (args.Length != 1)
            throw new ArgumentException("Missing benchmark specification");

        IBenchmark benchmark = args[0] switch
        {
            "AccountBad" => new AccountBad(),
            "BluetoothDriverBad" => new BluetoothDriverBad(),
            "BluetoothDriverBadTweaked" => new BluetoothDriverBadTweaked(),
            "Carter01Bad" => new Carter01Bad(),
            "CircularBufferBad" => new CircularBufferBad(),
            "Deadlock01Bad" => new Deadlock01Bad(),
            "Lazy01Bad" => new Lazy01Bad(),
            "QueueBad" => new QueueBad(),
            "ReorderBad3" => new ReorderBad3(),
            "ReorderBad4" => new ReorderBad4(),
            "ReorderBad5" => new ReorderBad5(),
            "ReorderBad10" => new ReorderBad10(),
            "ReorderBad20" => new ReorderBad20(),
            "ReorderBadTweaked3" => new ReorderBadTweaked3(),
            "ReorderBadTweaked4" => new ReorderBadTweaked4(),
            "ReorderBadTweaked5" => new ReorderBadTweaked5(),
            "ReorderBadTweaked10" => new ReorderBadTweaked10(),
            "ReorderBadTweaked20" => new ReorderBadTweaked20(),
            "StackBad" => new StackBad(),
            "TokenRingBad" => new TokenRingBad(),
            "TwoStageBadSmall" => new TwoStageBadSmall(),
            "TwoStageBad" => new TwoStageBad(),
            "TwoStageBad100" => new TwoStageBad100(),
            "TwoStageBadTweaked100" => new TwoStageBadTweaked100(),
            "WrongLockBad" => new WrongLockBad(),
            "WrongLockBad3" => new WrongLockBad3(),
            "WrongLockBadTweaked" => new WrongLockBadTweaked(),
            "WrongLockBadTweaked3" => new WrongLockBadTweaked3(),
            _ => throw new ArgumentException($"Invalid benchmark specification: {args[0]}")
        };

        try
        {
            try
            {
                benchmark.RunTest();
                Console.WriteLine(PassedMessage);
            }
            catch (AggregateException ex)
            {
                if (ex.InnerException != null)
                    throw ex.InnerException;
            }
        }
        catch (AssertionViolationException)
        {
            Console.WriteLine(AssertionViolatedMessage);
        }
    }
}