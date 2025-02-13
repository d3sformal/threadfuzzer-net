#include "cli_metadata_parser.hpp"
#include "cor_profiler.hpp"
#include "thread_safe_logger.hpp"

std::vector<const class_description*> cli_metadata_parser::parse_method_signature(PCCOR_SIGNATURE sig_blob, ULONG sig_size, const std::wstring& method_name)
{
    ULONG current_byte = 0;
    return parse_method_signature(sig_blob, current_byte);
}

const class_description* cli_metadata_parser::parse_type(PCCOR_SIGNATURE sig_blob, ULONG sig_size)
{
    ULONG current_byte = 0;
    return parse_next_type(sig_blob, current_byte);
}

std::vector<const class_description*> cli_metadata_parser::parse_method_signature(PCCOR_SIGNATURE sig_blob, ULONG& current_byte)
{
    ULONG call_conv = sig_blob[current_byte++];

    ULONG type_args_count = 0;
    if (call_conv & IMAGE_CEE_CS_CALLCONV_GENERIC)
        current_byte += CorSigUncompressData(sig_blob + current_byte, &type_args_count);

    ULONG args_count;
    current_byte += CorSigUncompressData(sig_blob + current_byte, &args_count);

    std::vector<const class_description*> result;
    auto return_type = parse_next_type(sig_blob, current_byte);

    for (UINT i = 0; i < args_count; i++)
        result.push_back(parse_next_type(sig_blob, current_byte));

    result.erase(std::ranges::remove(result, nullptr).begin(), result.end());

    result.push_back(return_type);

    return result;
}

const class_description* cli_metadata_parser::parse_next_type(PCCOR_SIGNATURE sigBlob, ULONG& current_byte)
{
    // ReSharper disable once CppIncompleteSwitchStatement
    // ReSharper disable once CppDefaultCaseNotHandledInSwitchStatement
    switch (auto type = static_cast<CorElementType>(sigBlob[current_byte++]))  // NOLINT(clang-diagnostic-switch-enum)
    {
    case ELEMENT_TYPE_VOID:       return profiler->find_class(L"System.Void");
    case ELEMENT_TYPE_BOOLEAN:    return profiler->find_class(L"System.Boolean");
    case ELEMENT_TYPE_I1:         return profiler->find_class(L"System.SByte");
    case ELEMENT_TYPE_U1:         return profiler->find_class(L"System.Byte");
    case ELEMENT_TYPE_I2:         return profiler->find_class(L"System.Int16");
    case ELEMENT_TYPE_U2:         return profiler->find_class(L"System.UInt16");
    case ELEMENT_TYPE_CHAR:       return profiler->find_class(L"System.Char");
    case ELEMENT_TYPE_I4:         return profiler->find_class(L"System.Int32");
    case ELEMENT_TYPE_U4:         return profiler->find_class(L"System.UInt32");
    case ELEMENT_TYPE_I8:         return profiler->find_class(L"System.Int64");
    case ELEMENT_TYPE_U8:         return profiler->find_class(L"System.UInt64");
    case ELEMENT_TYPE_R4:         return profiler->find_class(L"System.Single");
    case ELEMENT_TYPE_R8:         return profiler->find_class(L"System.Double");
    case ELEMENT_TYPE_OBJECT:     return profiler->find_class(L"System.Object");
    case ELEMENT_TYPE_STRING:     return profiler->find_class(L"System.String");
    case ELEMENT_TYPE_I:          return profiler->find_class(L"System.IntPtr");
    case ELEMENT_TYPE_U:          return profiler->find_class(L"System.UIntPtr");
    case ELEMENT_TYPE_TYPEDBYREF: return profiler->find_class(L"System.TypedReference");

    case ELEMENT_TYPE_CLASS:
    case ELEMENT_TYPE_VALUETYPE:
    {
        mdToken token;
        current_byte += CorSigUncompressToken(sigBlob + current_byte, &token);
        auto class_name = profiler->get_class_name_from_token(token, metadata_import);
        auto* class_desc = profiler->find_class(class_name);
        if (!class_desc) // ToDo proper class for this (deferred_load_class)
            class_desc = &profiler->register_fake_class(std::make_unique<base_class>(0, std::move(class_name), nullptr, class_description::POINTER_SIZE));
        return class_desc;
    }

    case ELEMENT_TYPE_FNPTR:
    {
        auto function_sig = parse_method_signature(sigBlob, current_byte);
        return &profiler->register_fake_class(std::make_unique<fnptr_class>(std::move(function_sig)));

    }
    case ELEMENT_TYPE_CMOD_REQD:
    case ELEMENT_TYPE_CMOD_OPT:
    {
        mdToken token;
        current_byte += CorSigUncompressToken(sigBlob + current_byte, &token);
        return parse_next_type(sigBlob, current_byte);
    }

    case ELEMENT_TYPE_PTR:
    case ELEMENT_TYPE_BYREF:
    {
        auto inner_type = parse_next_type(sigBlob, current_byte);
        if (auto existing_class = profiler->find_class(inner_type->get_name() + L"*"); existing_class)
            return existing_class;
        return &profiler->register_fake_class(std::make_unique<ptr_class>(*inner_type));
    }

    case ELEMENT_TYPE_SZARRAY: // simple array
    {
        auto arr_type = parse_next_type(sigBlob, current_byte);
        if (auto existing_class = profiler->find_class(arr_type->get_name() + L"[]"); existing_class)
            return existing_class;
        return &profiler->register_fake_class(std::make_unique<array_class>(*arr_type));
    }

    case ELEMENT_TYPE_ARRAY: // complex array
    {
        auto arr_type = parse_next_type(sigBlob, current_byte);
        // ToDo: extract array shape
        if (auto existing_class = profiler->find_class(arr_type->get_name() + L"[]"); existing_class)
            return existing_class;
        return &profiler->register_fake_class(std::make_unique<array_class>(*arr_type));
    }

    // I.e. if you had the class class Foo<T, U>, then for ELEMENT_TYPE_VAR 1 would refer to T and 2 would refer to U. 
    case ELEMENT_TYPE_VAR:
    case ELEMENT_TYPE_MVAR:
    {
        ULONG n;
        current_byte += CorSigUncompressData(sigBlob + current_byte, &n);
        return generic_var_ref_class::get_instance(n);
    }

    case ELEMENT_TYPE_GENERICINST:
    {
        auto* gen_type = parse_next_type(sigBlob, current_byte);

        ULONG n;
        current_byte += CorSigUncompressData(sigBlob + current_byte, &n);
        std::vector<const class_description*> types;
        for (ULONG i = 0; i < n; i++)
            types.push_back(parse_next_type(sigBlob, current_byte));

        std::wstring specialized_name = gen_type->get_name_simple();
        specialized_name.erase(std::ranges::find(specialized_name, L'`'), specialized_name.end());
        specialized_name += L"<";
        specialized_name += tmt::wstring_join(types | views::as_names, L", ");
        specialized_name += L">";

        if (auto existing_class = profiler->find_class(specialized_name); existing_class)
            return existing_class;

        profiler->log<logging_level::INFO>(L"Class '", specialized_name, L"' not loaded while parsing generic signature");
        return gen_type; // ToDo: or create fake generic class and supply types to it
    }

    default:
        throw profiler_error(L"Error: Invalid type being parsed " + std::to_wstring(type));
    }
}
