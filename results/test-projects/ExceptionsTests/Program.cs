using System;

namespace ExceptionsTests;

class CustomException : Exception
{
}

internal class Program
{
    static void Main(string[] args)
    {
        if (args.Length == 0 || !int.TryParse(args[0], out int variant))
        {
            Console.WriteLine($"Usage: {typeof(Program).Assembly.GetName().Name} <1-14>");
            return;
        }

        switch (variant)
        {
            case 1:
                Console.WriteLine("Must be detected. Main");
                throw new CustomException();
                    
            case 2:
                Console.WriteLine("Must be detected. Throw Main");
                Throw();
                break;

            case 3:
                Console.WriteLine("Must be detected. Main");
                try
                {
                    throw new CustomException();
                }
                catch (ArgumentException)
                {
                }
                break;
            case 4:
                Console.WriteLine("Must NOT be detected.");
                try
                {
                    throw new CustomException();
                }
                catch (CustomException)
                {
                }
                break;
            case 5:
                Console.WriteLine("Must NOT be detected.");
                try
                {
                    try
                    {
                        throw new CustomException();
                    }
                    catch (ArgumentException)
                    {
                    }
                }
                catch (CustomException)
                {
                }
                break;
            case 6:
                Console.WriteLine("Must NOT be detected.");
                try
                {
                    try
                    {
                        throw new CustomException();
                    }
                    catch (CustomException)
                    {
                    }
                }
                catch (CustomException)
                {
                }
                break;
            case 7:
                Console.WriteLine("Must NOT be detected.");
                try
                {
                    try
                    {
                        Throw();
                    }
                    catch (ArgumentException)
                    {
                    }
                }
                catch (CustomException)
                {
                }
                break;
            case 8:
                Console.WriteLine("Must NOT be detected.");
                try
                {
                    try
                    {
                        Throw();
                    }
                    catch (CustomException)
                    {
                    }
                }
                catch (CustomException)
                {
                }
                break;
            case 9:
                Console.WriteLine("Must be detected. TryThrowCatch Throw Main");
                TryThrowCatch<ArgumentException>();
                break;
            case 10:
                Console.WriteLine("Must NOT be detected.");
                TryThrowCatch<CustomException>();
                break;
            case 11:
                Console.WriteLine("Must NOT be detected.");
                try
                {
                    try
                    {
                        throw new CustomException();
                    }
                    catch (CustomException)
                    {
                        throw new CustomException();
                    }
                }
                catch (CustomException)
                {
                }
                break;
            case 12:
                Console.WriteLine("Must NOT be detected.");
                try
                {
                    try
                    {
                        try
                        {
                            throw new ArgumentException();
                        }
                        catch (CustomException)
                        {
                        }
                    }
                    catch (ArgumentException)
                    {
                        throw new CustomException();
                    }
                }
                catch (CustomException)
                {
                }
                break;
            case 13:
                Console.WriteLine("Must NOT be detected.");
                try
                {
                    try
                    {
                        try
                        {
                            throw new CustomException();
                        }
                        catch (CustomException)
                        {
                            throw new ArgumentException();
                        }
                    }
                    catch (ArgumentException)
                    {
                        throw new CustomException();
                    }
                }
                catch (CustomException)
                {
                }
                break;
            case 14:
                Console.WriteLine("Must be detected. Main");
                try
                {
                    try
                    {
                        throw new CustomException();
                    }
                    catch (CustomException)
                    {
                        throw new ArgumentException();
                    }
                }
                catch (ArgumentException)
                {
                    throw new CustomException();
                }
                break;

        }
    }

    private static void Throw()
    {
        throw new CustomException();
    }

    private static void TryThrowCatch<T>() where T : Exception
    {
        try
        {
            Throw();
        }
        catch (T)
        {
        }
    }
}
