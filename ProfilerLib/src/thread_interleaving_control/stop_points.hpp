#pragma once

#include "../net_types.hpp"

#include "../utils/wstring_split.hpp"

#include <vector>
#include <regex>
#include <string>
#include <ranges>

class stop_point
{
public:
    class method_desc
    {
        std::wregex type_rgx;
        std::wregex method_rgx;
    public:
        explicit method_desc(const std::wstring& desc_str)
        {
            auto space_index = desc_str.find(L' ');

            auto type = space_index != std::wstring::npos ? desc_str.substr(0, space_index) : desc_str;
            auto method = space_index != std::wstring::npos ? desc_str.substr(space_index + 1) : L"Main";

            type_rgx = std::wregex(type);
            method_rgx = std::wregex(method);
        }

        [[nodiscard]] bool matches(const function_spec* function) const
        {
            return matches(function->get_defining_class()->get_name(), function->get_name());
        }

        [[nodiscard]] bool matches(const std::wstring& type_name, const std::wstring& method_name) const
        {
            return std::regex_match(type_name, type_rgx) && std::regex_match(method_name, method_rgx);
        }
    };
private:
    // ReSharper disable once IdentifierTypo
    std::vector<method_desc> stop_point_descs;

public:
    explicit stop_point(const std::wstring& str)
    {
        static std::wregex when_in_rgx { L" +when_in +" };

        auto method_descs = tmt::wstring_split(str, when_in_rgx);
        for (auto& method_desc_str : method_descs)
        {
            stop_point_descs.emplace_back(std::move(method_desc_str));
        }
    }

    [[nodiscard]] bool matches(const std::vector<const function_spec*>& call_stack) const
    {
        if (!stop_point_descs.front().matches(call_stack.back()))
            return false;

        auto point_iter = stop_point_descs.begin();
        for (auto stack_iter : std::ranges::reverse_view(call_stack))
        {
            if (point_iter->matches(stack_iter))
                if (++point_iter == stop_point_descs.end())
                    return true;
        }
        return point_iter == stop_point_descs.end();
    }

    [[nodiscard]] bool contains(const std::wstring& type_name, const std::wstring& method_name) const
    {
        return std::ranges::any_of(stop_point_descs, [&](const method_desc& md) { return md.matches(type_name, method_name); });
    }
};

class stop_points
{
    std::vector<stop_point> m_stop_points;

public:
    explicit stop_points(const std::vector<std::wstring>& stop_points)
    {
        for (const auto& str : stop_points)
            m_stop_points.emplace_back(str);
    }

    [[nodiscard]] bool matches(const std::vector<const function_spec*>& call_stack) const
    {
        return std::ranges::any_of(m_stop_points, [&](const stop_point& point)
        {
            return point.matches(call_stack);
        });
    }
};
