#pragma once
#include <string>
#include <regex>
#include <unordered_map>
#include <vector>
#include <ranges>

class config_file
{
    inline static std::wstring default_value;
    inline static std::vector<std::wstring> default_values;

    static config_file instance;

public:
    static config_file& load_file(const std::string& str);

    static const config_file& get_instance()
    {
        return instance;
    }

    [[nodiscard]] const std::wstring& get_value(const std::wstring& key) const
    {
        if (auto iter = data.find(key); iter != data.end())
            return iter->second[0];
        return default_value;
    }

    template<typename T>
    T get_value(const std::wstring& key) const;

    template<>
    [[nodiscard]] int get_value(const std::wstring& key) const
    {
        auto value_str = get_value(key);
        try
        {
            return std::stoi(value_str);
        }
        catch(...)
        {
            return 0;
        }
    }

    [[nodiscard]] const std::vector<std::wstring>& get_values(const std::wstring& key) const
    {
        if (auto iter = data.find(key); iter != data.end())
            return iter->second;
        return default_values;
    }

    [[nodiscard]] bool empty() const
    {
        return data.empty();
    }

private:
    std::unordered_map<std::wstring, std::vector<std::wstring>> data;

    void new_data_item(std::wstring&& key, std::wstring&& value)
    {
        auto result = data.try_emplace(std::move(key));
        auto& vector = result.first->second;
        if (!result.second)
            vector.clear();

        vector.push_back(std::move(value));
    }

    void update_data_item(std::wstring&& key, std::wstring&& value)
    {
        auto& vector = data.try_emplace(std::move(key)).first->second;
        vector.push_back(std::move(value));
    }
};
