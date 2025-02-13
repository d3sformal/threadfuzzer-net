#pragma once

#include <cor.h>
#include <corprof.h>

#include <string>
#include <vector>
#include <algorithm>
#include <optional>
#include <unordered_map>
#include <ranges>

#include "utils/wstring_join.hpp"

class class_description
{
public:
#ifdef _M_IX86 // x86
    static constexpr std::size_t POINTER_SIZE = 4;
#else
    static constexpr std::size_t POINTER_SIZE = 8;
#endif
    static constexpr std::size_t UNKNOWN_SIZE = static_cast<std::size_t>(-1);
    static const class_description* UNKNOWN;

    static std::size_t guess_size_from_name(const std::wstring& name)
    {
        static auto* builtin_sizes = new std::unordered_map<std::wstring, std::size_t> {
            {    L"System.Void", 0 },
            { L"System.Boolean", 1 },
            {   L"System.SByte", 1 },
            {    L"System.Byte", 1 },
            {   L"System.Int16", 2 },
            {  L"System.UInt16", 2 },
            {    L"System.Char", 2 },
            {   L"System.Int32", 4 },
            {  L"System.UInt32", 4 },
            {   L"System.Int64", 8 },
            {  L"System.UInt64", 8 },
            {  L"System.Single", 4 },
            {  L"System.Double", 8 },
            {  L"System.Object", POINTER_SIZE },
            {  L"System.String", POINTER_SIZE },
            {  L"System.IntPtr", POINTER_SIZE },
            { L"System.UIntPtr", POINTER_SIZE },
            { L"System.TypedRef", POINTER_SIZE },
        };

        if (auto iter = builtin_sizes->find(name); iter != builtin_sizes->end())
            return iter->second;
        return UNKNOWN_SIZE;
    }

    class_description(ClassID class_id, std::size_t byte_size) : internal_handle(class_id), byte_size(byte_size)
    {
    }

    class_description(const class_description&) = default;
    class_description& operator=(const class_description&) = default;
    class_description(class_description&&) = default;
    class_description& operator=(class_description&&) = default;
    virtual ~class_description() = default;

    [[nodiscard]] const std::wstring& get_name() const
    {
        if (!cached_name)
            cached_name = get_name_internal();

        return cached_name.value();
    }

    [[nodiscard]] virtual const std::wstring& get_name_simple() const = 0;

    [[nodiscard]] ClassID get_internal_handle() const
    {
        return internal_handle;
    }

    [[nodiscard]] std::size_t get_size() const
    {
        return byte_size;
    }

protected:
    virtual std::wstring get_name_internal() const = 0;

private:
    ClassID internal_handle;
    std::size_t byte_size;

    mutable std::optional<std::wstring> cached_name;
};

namespace views
{
    static constexpr auto as_names = std::views::transform([](const class_description* desc) { return desc->get_name(); });
}

class base_class : public class_description
{
protected:
    std::wstring name;
private:
    const class_description* parent_class;
public:
    explicit base_class(ClassID class_id, std::wstring&& name, const class_description* parent_class, std::size_t byte_size)
        : class_description(class_id, byte_size)
        , name(std::move(name)), parent_class(parent_class)
    {
    }

    const std::wstring& get_name_simple() const override
    {
        return name;
    }

    const class_description* get_parent_class() const
    {
        return parent_class;
    }

protected:
    std::wstring get_name_internal() const override
    {
        std::wstring result = name;
        result.erase(std::ranges::find(result, L'`'), result.end());
        return result;
    }
};

class generic_class final : public base_class
{
    std::vector<const class_description*> generic_classes;
public:
    explicit generic_class(ClassID class_id, std::wstring&& class_name, const class_description* parent_class, std::size_t byte_size)
        : base_class(class_id, std::move(class_name), parent_class, byte_size)
    {
    }

    void add_class(const class_description& desc)
    {
        generic_classes.push_back(&desc);
    }

    const std::wstring& get_name_simple() const override
    {
        return name;
    }

protected:
    std::wstring get_name_internal() const override
    {
        std::wstring name = base_class::get_name_internal();
        name += L"<";
        name += tmt::wstring_join(generic_classes | views::as_names, L", ");
        name += L">";
        return name;
    }
};

class array_class final : public class_description
{
    const class_description* base_class;
    std::size_t array_rank;

public:
    explicit array_class(const class_description& base, std::size_t rank = 1) : class_description(base.get_internal_handle(), POINTER_SIZE), base_class(&base), array_rank(rank)
    {
    }

    const std::wstring& get_name_simple() const override
    {
        return get_name();
    }

protected:
    std::wstring get_name_internal() const override
    {
        std::wstring name = base_class->get_name();
        name += L"[";
        name += std::wstring(array_rank - 1, L',');
        name += L"]";
        return name;
    }
};

class ptr_class final : public class_description
{
    const class_description* base_class;

public:
    explicit ptr_class(const class_description& base) : class_description(base.get_internal_handle(), POINTER_SIZE), base_class(&base)
    {
    }

    const std::wstring& get_name_simple() const override
    {
        return get_name();
    }

protected:
    std::wstring get_name_internal() const override
    {
        return base_class->get_name() + L"*";
    }
};

class generic_var_ref_class final : public class_description
{
    std::size_t index;
    explicit generic_var_ref_class(std::size_t index) : class_description(0, UNKNOWN_SIZE), index(index)
    {
    }

protected:
    std::wstring get_name_internal() const override
    {
        return L"T" + std::to_wstring(index);
    }

public:
    static class_description* get_instance(std::size_t index)
    {
        static auto* instances = new std::vector<class_description*>{};
        for (std::size_t i = instances->size(); i <= index; ++i)
            instances->push_back(new generic_var_ref_class(i));
        return instances->at(index);
    }

    const std::wstring& get_name_simple() const override
    {
        return get_name();
    }

    std::size_t get_index() const
    {
        return index;
    }
};

class fnptr_class final : public class_description
{
    std::vector<const class_description*> signature;
public:
    explicit fnptr_class(std::vector<const class_description*>&& signature) : class_description(0, POINTER_SIZE), signature(std::move(signature))
    {
    }
protected:
    std::wstring get_name_internal() const override
    {
        return L"<fnptr>"; // ToDo: of what
    }

public:
    [[nodiscard]] const std::wstring& get_name_simple() const override
    {
        return get_name();
    }
};

class function_spec
{
    FunctionID internal_handle;
    std::wstring name;
    const class_description* defining_class;
    const class_description* return_type;
    std::vector<const class_description*> arg_types;
    bool m_is_static;

    mutable std::optional<std::wstring> signature_cache;
    mutable std::optional<std::wstring> pretty_info_cache;
    mutable std::optional<std::pmr::wstring> pmr_pretty_info_cache;

public:
    static const function_spec* UNKNOWN;

    explicit function_spec(FunctionID function_id, std::wstring&& name, const class_description* defining_class, const class_description* return_type, std::vector<const class_description*>&& arg_types, bool is_static)
        : internal_handle(function_id), name(std::move(name)), defining_class(defining_class), return_type(return_type), arg_types(std::move(arg_types)), m_is_static(is_static)
    {
    }

    [[nodiscard]] const std::wstring& get_name() const
    {
        return name;
    }

    [[nodiscard]] const std::wstring& get_signature() const
    {
        if (!signature_cache.has_value())
        {
            std::wstring sig = return_type->get_name() + L" " + name + L"(";
            sig += tmt::wstring_join(arg_types | views::as_names, L", ");
            sig += L")";
            signature_cache = std::move(sig);
        }
        return signature_cache.value();
    }

    [[nodiscard]] const std::pmr::wstring& get_pretty_info(std::pmr::memory_resource* mem_res) const
    {
        return get_pretty_info<std::pmr::polymorphic_allocator<wchar_t>>(pmr_pretty_info_cache, mem_res);
    }

    [[nodiscard]] const std::wstring& get_pretty_info() const
    {
        return get_pretty_info(pretty_info_cache);
    }

private:
    template<typename Alloc>
    [[nodiscard]] const std::basic_string<wchar_t, std::char_traits<wchar_t>, Alloc>& get_pretty_info
    (
        std::optional<std::basic_string<wchar_t, std::char_traits<wchar_t>, Alloc>>& cache,
        Alloc alloc = Alloc{}
    ) const
    {
        using wstring = std::basic_string<wchar_t, std::char_traits<wchar_t>, Alloc>;
        if (!cache.has_value())
        {
            wstring info(alloc);
            info.append(defining_class->get_name());
            info.append(L".");
            info.append(name);
            info.append(L"(");
            info.append(tmt::wstring_join(arg_types | views::as_names, L", ", alloc));
            info.append(L")");
            cache = std::move(info);
        }
        return cache.value();
    }

public:
    [[nodiscard]] FunctionID get_internal_handle() const
    {
        return internal_handle;
    }

    [[nodiscard]] const class_description* get_defining_class() const
    {
        return defining_class;
    }

    [[nodiscard]] const class_description* get_return_type() const
    {
        return return_type;
    }

    [[nodiscard]] const std::vector<const class_description*>& get_arguments() const
    {
        return arg_types;
    }

    [[nodiscard]] bool is_static() const
    {
        return m_is_static;
    }
};
