#include "config_file.hpp"

#include <fstream>
#include <iostream>
#include <ranges>
#include <filesystem>

config_file config_file::instance;

config_file& config_file::load_file(const std::wstring& str)
{
    std::wregex include_regex(LR"(^@include\s+(.*)$)");
    std::wregex line_regex(LR"(^(\w+)\s+(\+?\=)\s*(.*)\s*$)");
    std::wifstream file(str);
    if (!file)
        return instance;

    std::wstring line;
    std::size_t line_number = 0;
    while (std::getline(file, line))
    {
        ++line_number;
        if (line.starts_with(L"#") || line.empty())
            continue;

        std::wstring trimmed_line(line.begin(), std::ranges::find(line, L'#'));
        while (!trimmed_line.empty() && trimmed_line.back() == L' ')
            trimmed_line.pop_back();

        std::wsmatch match;
        if (std::regex_match(trimmed_line, match, include_regex))
        {
            std::filesystem::path current_conf_path{ str };
            std::filesystem::path including_path{ match[1] };
            if (including_path.is_absolute())
                load_file(match[1]);
            else
                load_file(current_conf_path.parent_path() / including_path);
        }
        else if (std::regex_match(trimmed_line, match, line_regex))
        {
            std::wstring key = match[1];
            std::wstring op = match[2];
            std::wstring value = match[3];

            if (op == L"+=")
                instance.update_data_item(std::move(key), std::move(value));
            else
                instance.new_data_item(std::move(key), std::move(value));
        }
        else
            std::wcerr << L"Error on line " << line_number << L" in file '" << str << "'" << std::endl;
    }
    return instance;
}
