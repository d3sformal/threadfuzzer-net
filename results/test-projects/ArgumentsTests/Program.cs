namespace ArgumentsTests;

class Program
{
    static void FunctionTest_bool(bool arg) { }
    static void FunctionTest_byte(byte arg) { }
    static void FunctionTest_sbyte(sbyte arg) { }
    static void FunctionTest_char(char arg) { }
    static void FunctionTest_decimal(decimal arg) { }
    static void FunctionTest_double(double arg) { }
    static void FunctionTest_float(float arg) { }
    static void FunctionTest_int(int arg) { }
    static void FunctionTest_nint(nint arg) { }
    static void FunctionTest_nuint(nuint arg) { }
    static void FunctionTest_uint(uint arg) { }
    static void FunctionTest_long(long arg) { }
    static void FunctionTest_ulong(ulong arg) { }
    static void FunctionTest_short(short arg) { }
    static void FunctionTest_ushort(ushort arg) { }
    static void FunctionTest_object(object arg) { }
    static void FunctionTest_string(string arg) { }

    static void Main(string[] args)
    {
        FunctionTest_bool(default);
        FunctionTest_byte(default);
        FunctionTest_sbyte(default);
        FunctionTest_char(default);
        FunctionTest_decimal(default);
        FunctionTest_double(default);
        FunctionTest_float(default);
        FunctionTest_int(default);
        FunctionTest_nint(default);
        FunctionTest_nuint(default);
        FunctionTest_uint(default);
        FunctionTest_long(default);
        FunctionTest_ulong(default);
        FunctionTest_short(default);
        FunctionTest_ushort(default);
        FunctionTest_object(default);
        FunctionTest_string(default);
    }
}
