#include <named_pipe.hpp>

#include <iostream>
#include <locale>
#include <codecvt>
#include <string>

#include <Windows.h>

// https://docs.microsoft.com/en-us/windows/console/clearing-the-screen
void cls()
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    SMALL_RECT scrollRect;
    COORD scrollTarget;
    CHAR_INFO fill;

    // Get the number of character cells in the current buffer.
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
    {
        return;
    }

    // Scroll the rectangle of the entire buffer.
    scrollRect.Left = 0;
    scrollRect.Top = 0;
    scrollRect.Right = csbi.dwSize.X;
    scrollRect.Bottom = csbi.dwSize.Y;

    // Scroll it upwards off the top of the buffer with a magnitude of the entire height.
    scrollTarget.X = 0;
    scrollTarget.Y = (SHORT) (0 - csbi.dwSize.Y);

    // Fill with empty spaces with the buffer's default text attribute.
    fill.Char.UnicodeChar = TEXT(' ');
    fill.Attributes = csbi.wAttributes;

    // Do the scroll
    ScrollConsoleScreenBuffer(hConsole, &scrollRect, NULL, scrollTarget, &fill);

    // Move the cursor to the top left corner too.
    csbi.dwCursorPosition.X = 0;
    csbi.dwCursorPosition.Y = 0;

    SetConsoleCursorPosition(hConsole, csbi.dwCursorPosition);
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 2)
    {
        std::wcout << L"Usage " << argv[0] << " pipe_name" << std::endl;
        return 1;
    }
    try
    {
        named_pipe pipe(argv[1], 4096, named_pipe::CONNECT);
        while (true)
        {
            auto message = pipe.read();

            if (message == named_pipe::EOT)
                break;
            if (message == named_pipe::READ)
            {
                std::wstring reply;
                std::getline(std::wcin, reply);
                pipe.write(reply);
                cls();
            }
            else if (message == named_pipe::WRITE)
            {
                std::wcout << pipe.read() << std::endl;
            }
            else
            {
                throw std::runtime_error("Invalid message specifier");
            }
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << std::endl << ex.what() << std::endl;
        throw;
    }
}
