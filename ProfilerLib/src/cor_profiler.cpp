#include "cor_profiler.hpp"

#include "argument_data.hpp"
#include "stack_info.hpp"
#include "thread_local_storage.hpp"
#include "cli_metadata_parser.hpp"

thread_local thread_local_storage tls;

HRESULT cor_profiler::ThreadCreated(ThreadID threadId)
{
    log<logging_level::INFO>(L"[ThreadCreated] ", threadId);
    tls.thr_counted_id = ++internal_id_counter;
    return S_OK;
}

HRESULT cor_profiler::ThreadAssignedToOSThread(ThreadID managedThreadId, DWORD osThreadId)
{
    log<logging_level::INFO>(L"[ThreadAssignedToOSThread] ", managedThreadId, L" -> ", osThreadId);
    thread_mappings.try_emplace(osThreadId, managedThreadId, GetCurrentThreadId(), std::this_thread::get_id(), tls.thr_counted_id);

    return S_OK;
}

HRESULT cor_profiler::ThreadNameChanged(ThreadID threadId, ULONG cchName, WCHAR name[])
{
    std::wstring new_name(name, name + cchName);
    log<logging_level::INFO>(L"[ThreadNameChanged] ", threadId, " ", new_name);

    SetThreadDescription(GetCurrentThread(), new_name.c_str());

    return S_OK;
}

HRESULT cor_profiler::ClassLoadFinished(ClassID classId, HRESULT hrStatus) try
{
    auto& cls = register_class(classId);
    log<logging_level::VERBOSE>(L"[ClassLoadFinished] ", cls.get_name(), L" [", cls.get_size(), L"B]");

    return S_OK;
}
COR_CATCH_HANDLER_DEFAULT

HRESULT cor_profiler::ObjectAllocated(ObjectID objectId, ClassID classId)
{
    auto class_name = get_class_info(classId)->get_name();
    log<logging_level::VERBOSE>(L"[ObjectAllocated] ", class_name, " ", is_value_type(classId));

    return S_OK;
}

std::vector<argument_data> cor_profiler::get_argument_data(const function_spec* function, COR_PRF_ELT_INFO elt_info) const
{
    ULONG argument_info_size = 0;
    COR_PRF_FRAME_INFO frame_info;
    COR_ASSERT(cor_profiler_info->GetFunctionEnter3Info(function->get_internal_handle(), elt_info, &frame_info, &argument_info_size, nullptr), profiler_error::H_ERROR_INSUFFICIENT_BUFFER);

    auto argument_info_storage = std::make_unique<std::byte[]>(argument_info_size);
    auto argument_info_ptr = reinterpret_cast<COR_PRF_FUNCTION_ARGUMENT_INFO*>(argument_info_storage.get());
    COR_SANITIZE(cor_profiler_info->GetFunctionEnter3Info(function->get_internal_handle(), elt_info, &frame_info, &argument_info_size, argument_info_ptr));
    if (argument_info_size > 0)
    {
        if (function->get_arguments().size() + (function->is_static() ? 0 : 1) != argument_info_ptr->numRanges)
            throw profiler_error(L"ERROR: Non matching arguments sizes for function " + function->get_signature() + L" of class " + function->get_defining_class()->get_name());

        std::vector<argument_data> arg_data;
        for (std::size_t i = 0; i < argument_info_ptr->numRanges; ++i) // ToDo: assuming this is per argument
            arg_data.emplace_back(argument_info_ptr->ranges[i].startAddress, argument_info_ptr->ranges[i].length);

        return arg_data;
    }
    throw profiler_error(L"ERROR: 0 is size of args");
}

std::wstring cor_profiler::get_class_name(ClassID class_id, IMetaDataImport* metadata_import, mdTypeDef metadata)
{
    ULONG count;
    COR_SANITIZE(metadata_import->GetTypeDefProps(metadata, nullptr, 0, &count, nullptr, nullptr));
    std::wstring name;
    name.resize(count - 1); // null-terminated
    COR_SANITIZE(metadata_import->GetTypeDefProps(metadata, name.data(), count, nullptr, nullptr, nullptr));
    return name;
}

std::wstring cor_profiler::get_class_name_from_token(mdToken token, IMetaDataImport2* metadata_import)
{
    auto inner_lambda = [this](mdToken token, IMetaDataImport2* metadata_import)
    {
        std::wstring name;
        name.resize(250);
        ULONG real_size;
        mdToken nested = 0;
        if (TypeFromToken(token) == mdtTypeRef)
            COR_SANITIZE(metadata_import->GetTypeRefProps(token, nullptr, name.data(), name.size(), &real_size));
        else if (TypeFromToken(token) == mdtTypeDef)
            COR_SANITIZE(metadata_import->GetTypeDefProps(token, name.data(), name.size(), &real_size, nullptr, &nested));
        else if (TypeFromToken(token) == mdtTypeSpec)
        {
            PCCOR_SIGNATURE sig_blob;
            ULONG sig_size;
            COR_SANITIZE(metadata_import->GetTypeSpecFromToken(token, &sig_blob, &sig_size));
            cli_metadata_parser parser(this, metadata_import);
            auto* type = parser.parse_type(sig_blob, sig_size);
            return std::pair{ type->get_name(), 0u };
        }
        else
            throw profiler_error(L"Unknown TokenType, " + std::to_wstring(token));

        name.resize(real_size - 1);
        return std::pair{ name, nested };
    };

    auto [name, nested] = inner_lambda(token, metadata_import);

    if (nested == 0)
        return name;

    auto nestedName = inner_lambda(nested, metadata_import).first;
    if (nestedName == L"System.Enum" || nestedName == L"System.ValueType")
        return nestedName; // BYREF hack for value types
    return name;
}

HRESULT cor_profiler::MethodEntry(const function_spec* function, COR_PRF_ELT_INFO elt_info) const try
{
    auto& class_name = function->get_defining_class()->get_name();
    auto& function_name = function->get_name();

    DWORD event_mask;
    COR_SANITIZE(cor_profiler_info->GetEventMask(&event_mask));

    std::vector<argument_data> argument_data;
    if (event_mask & COR_PRF_ENABLE_FUNCTION_ARGS)
        argument_data = get_argument_data(function, elt_info);

    log<logging_level::VERBOSE>(L"[MethodEntry] Class: ", class_name, L", Method: ", function_name);
    //log<logging_level::DEBUG>(L"  full sig: ", function->get_return_type()->get_name(), L" ", function->get_pretty_info(), L" {");

    if (log_target.is_logging<logging_level::DEBUG>() && event_mask & COR_PRF_ENABLE_FUNCTION_ARGS)
    {
        auto log_f = [this]<typename T>(T && value) { log<logging_level::DEBUG>(L"    ", std::forward<T>(value)); };

        if (!function->is_static())
            log_f(argument_data[0].as<net_reference>());

        for (std::size_t i = (!function->is_static() ? 1 : 0); i < argument_data.size(); ++i)
        {
            auto arg_name = function->get_arguments()[i - (!function->is_static() ? 1 : 0)]->get_name_simple();

            if (!args::dispatch(log_f, argument_data[i], arg_name))
                log<logging_level::WARN>(L"    Unknown value (", arg_name, ")");
        }

        log<logging_level::DEBUG>(L"}");
    }

    function_entry_hook(function, argument_data);

    return S_OK;
}
COR_CATCH_HANDLER_DEFAULT

HRESULT cor_profiler::MethodLeave(const function_spec* function, COR_PRF_ELT_INFO elt_info) const
{
    auto& class_name = function->get_defining_class()->get_name();
    auto& function_name = function->get_name();

    log<logging_level::VERBOSE>(L"[MethodLeave] Class: ", class_name, L", Method: ", function_name);

    function_leave_hook(function);

    return S_OK;
}

UINT_PTR cor_profiler::compute_native_offset(const function_spec* function, UINT_PTR ip, std::pmr::memory_resource* mem_resource) const
{
    std::size_t size;
    COR_SANITIZE(cor_profiler_info->GetCodeInfo2(function->get_internal_handle(), 0, &size, nullptr));

    std::pmr::vector<COR_PRF_CODE_INFO> code_infos(size, mem_resource);
    COR_SANITIZE(cor_profiler_info->GetCodeInfo2(function->get_internal_handle(), size, &size, code_infos.data()));

    for (const auto& code_info : code_infos)
    {
        if (code_info.startAddress <= ip && ip < code_info.startAddress + code_info.size)
            return ip - code_info.startAddress;
    }

    return static_cast<std::size_t>(-1);
}

UINT_PTR cor_profiler::compute_il_offset(const function_spec* function, UINT_PTR native_offset, std::pmr::memory_resource* mem_resource) const
{
    std::size_t count;
    COR_SANITIZE(cor_profiler_info->GetILToNativeMapping(function->get_internal_handle(), 0, &count, nullptr));

    std::pmr::vector<COR_DEBUG_IL_TO_NATIVE_MAP> il_native_map(count, mem_resource);
    COR_SANITIZE(cor_profiler_info->GetILToNativeMapping(function->get_internal_handle(), count * sizeof(COR_DEBUG_IL_TO_NATIVE_MAP), &count, il_native_map.data()));

    for (const auto& map_item : il_native_map)
    {
        if (map_item.nativeStartOffset <= native_offset && native_offset < map_item.nativeEndOffset)
            return map_item.ilOffset;
    }
    return static_cast<std::size_t>(-1);
}

HRESULT cor_profiler::StackSnapshot(const function_spec* function, UINT_PTR ip, COR_PRF_FRAME_INFO frame_info, void* client_data) const
{
    stack_info& stack = *static_cast<stack_info*>(client_data);

    if (!function)
        return S_OK;

    std::size_t native_offset = compute_native_offset(function, ip, stack.get_memory_resource());
    std::size_t il_offset = compute_il_offset(function, native_offset, stack.get_memory_resource());

    return stack.add_stack_item(function, native_offset, il_offset) ? S_OK : S_FALSE;
}

HRESULT cor_profiler::ExceptionThrown(ObjectID thrown_object_id)
{
    ClassID class_id;
    COR_SANITIZE(cor_profiler_info->GetClassFromObject(thrown_object_id, &class_id));

    current_exception.thrown_type = get_class_info(class_id);

    return S_OK;
}

HRESULT cor_profiler::ExceptionUnwindFunctionEnter(FunctionID functionId)
{
    current_exception.unwound_function = get_function_info(functionId);

    return S_OK;
}

HRESULT cor_profiler::ExceptionUnwindFunctionLeave()
{
    if (const std::wregex entry_point_rgx(config_file::get_instance().get_value(L"entry_point"));
        current_exception.unwound_function->get_name() == L"Main" &&
        std::regex_match(current_exception.unwound_function->get_defining_class()->get_name(), entry_point_rgx)
       )
    {
        // Unhandled Exception

        log<logging_level::WARN>(L"Unhandled .NET exception on Main leave: ", current_exception.thrown_type->get_name());

        current_exception.unwound_function = nullptr;
    }
    return S_OK;
}

HRESULT cor_profiler::ExceptionCatcherLeave()
{
    current_exception.thrown_type = nullptr;
    current_exception.unwound_function = nullptr;
    return S_OK;
}

const class_description& cor_profiler::register_class(ClassID class_id)
{
    if (auto iter = classes.find(class_id); iter != classes.end())
        return *iter->second;
    std::lock_guard guard(cls_mutex);

    ClassID base_class_def;
    ULONG arr_rank;
    if (cor_profiler_info->IsArrayClass(class_id, nullptr, &base_class_def, &arr_rank) == S_OK)
    {
        const class_description& registered_base = register_class(base_class_def);
        return *(classes[class_id] = std::make_unique<array_class>(registered_base, arr_rank));
    }

    std::size_t generic_classes_count;
    COR_SANITIZE(cor_profiler_info->GetClassIDInfo2(class_id, nullptr, nullptr, nullptr, 0, &generic_classes_count, nullptr));
    ModuleID module_id;
    mdTypeDef metadata;
    ClassID parent_class_net;
    std::vector<ClassID> generic_classes(generic_classes_count);
    COR_SANITIZE(cor_profiler_info->GetClassIDInfo2(class_id, &module_id, &metadata, &parent_class_net, generic_classes_count, &generic_classes_count, generic_classes.data()));

    IMetaDataImport2* metadata_import;
    COR_SANITIZE(cor_profiler_info->GetModuleMetaData(module_id, ofNoTransform, IID_IMetaDataImport2, reinterpret_cast<IUnknown**>(&metadata_import)));

    std::wstring name = get_class_name(class_id, metadata_import, metadata);

    while (true)
    {
        mdTypeDef enclosing_class_metadata;
        auto hresult = metadata_import->GetNestedClassProps(metadata, &enclosing_class_metadata);
        if (hresult == profiler_error::H_ERROR_RECORD_NOT_FOUND)
            break;
        COR_ASSERT(hresult, S_OK);

        std::wstring enclosing_name = get_class_name_from_token(enclosing_class_metadata, metadata_import);
        enclosing_name += L"." + std::move(name);
        name = std::move(enclosing_name);
        metadata = enclosing_class_metadata;
    }

    const class_description* parent_class = nullptr;
    if (parent_class_net)
        parent_class = &register_class(parent_class_net);

    if (generic_classes_count)
    {
        auto generic_cls = std::make_unique<generic_class>(class_id, std::move(name), parent_class, get_class_size(class_id));
        for (const auto& generic_class : generic_classes)
            generic_cls->add_class(register_class(generic_class));
        return *(classes[class_id] = std::move(generic_cls));
    }

    return *(classes[class_id] = std::make_unique<base_class>(class_id, std::move(name), parent_class, get_class_size(class_id)));
}

const function_spec& cor_profiler::register_function(FunctionID function_id, COR_PRF_ELT_INFO elt_info)
{
    std::lock_guard guard(fun_mutex);
    if (auto iter = functions.find(function_id); iter != functions.end())
        return *iter->second;

    ULONG argument_info_size = 0;
    COR_PRF_FRAME_INFO frame_info = 0;
    if (elt_info)
        COR_ASSERT(cor_profiler_info->GetFunctionEnter3Info(function_id, elt_info, &frame_info, &argument_info_size, nullptr), profiler_error::H_ERROR_INSUFFICIENT_BUFFER, S_OK);

    ClassID defining_class_net;
    ModuleID module_id;
    mdToken md_token;
    COR_SANITIZE(cor_profiler_info->GetFunctionInfo2(function_id, frame_info, &defining_class_net, &module_id, &md_token, 0, nullptr, nullptr));

    auto* defining_class = &register_class(defining_class_net);

    IMetaDataImport2* metadata_import;
    COR_SANITIZE(cor_profiler_info->GetModuleMetaData(module_id, ofNoTransform, IID_IMetaDataImport2, reinterpret_cast<IUnknown**>(&metadata_import)));

    ULONG name_count;
    DWORD flags;
    COR_SANITIZE(metadata_import->GetMethodProps(md_token, nullptr, nullptr, 0, &name_count, &flags, nullptr, nullptr, nullptr, nullptr));
    std::wstring method_name;
    method_name.resize(name_count - 1); // null-terminated
    PCCOR_SIGNATURE sig_blob;
    ULONG sig_size;
    COR_SANITIZE(metadata_import->GetMethodProps(md_token, nullptr, method_name.data(), name_count, &name_count, nullptr, &sig_blob, &sig_size, nullptr, nullptr));

    std::vector<const class_description*> arg_types;
    try
    {
        cli_metadata_parser parser(this, metadata_import);
        arg_types = parser.parse_method_signature(sig_blob, sig_size, method_name);
    }
    catch (profiler_error& err)
    {
        err.add_msg(L"While parsing signature for " + method_name + L" of " + defining_class->get_name());
        throw;
    }
    auto* ret_type = arg_types.back();
    arg_types.pop_back();
    bool is_static = flags & CorMethodAttr::mdStatic;

    auto function = std::make_unique<function_spec>(function_id, std::move(method_name), defining_class, ret_type, std::move(arg_types), is_static);

    return *(functions[function_id] = std::move(function));
}

bool cor_profiler::should_hook(FunctionID function_id)
{
    ClassID defining_class_net;
    ModuleID module_id;
    mdToken md_token;

    COR_SANITIZE(cor_profiler_info->GetFunctionInfo2(function_id, 0, &defining_class_net, &module_id, &md_token, 0, nullptr, nullptr));

    IMetaDataImport2* metadata_import;
    COR_SANITIZE(cor_profiler_info->GetModuleMetaData(module_id, ofNoTransform, IID_IMetaDataImport2, reinterpret_cast<IUnknown**>(&metadata_import)));

    ULONG name_count;
    DWORD flags;
    COR_SANITIZE(metadata_import->GetMethodProps(md_token, nullptr, nullptr, 0, &name_count, &flags, nullptr, nullptr, nullptr, nullptr));
    std::wstring method_name;
    method_name.resize(name_count - 1); // null-terminated
    PCCOR_SIGNATURE sig_blob;
    ULONG sig_size;
    COR_SANITIZE(metadata_import->GetMethodProps(md_token, nullptr, method_name.data(), name_count, &name_count, nullptr, &sig_blob, &sig_size, nullptr, nullptr));

    if (defining_class_net)
    {
        const class_description* defining_class = &register_class(defining_class_net);

        // Entry point
        if (is_entry_point(method_name, defining_class->get_name()))
            return true;

        // All entry-point's methods
        if (std::regex_match(defining_class->get_name(), std::wregex(config_file::get_instance().get_value(L"entry_point"))))
            return true;

        // All stop points
        if (std::ranges::any_of(stop_points, [&](const stop_point& sp) { return sp.contains(defining_class->get_name(), method_name); }))
            return true;

        static std::vector<std::pair<std::wstring, std::wstring>> extra_functions =
        {
            { L"System.Threading.Tasks.Task",  L"Execute"},
            { L"Microsoft.VisualStudio.TestPlatform.MSTestFramework.TestMethodRunner", L"ExecuteInternal" },
            { L"NUnit.Framework.Internal.Reflect", L"InvokeMethod"}, // ToDo: this is single test, do for all tests
            // { L"Xunit.XunitFrontController", L"RunTests" } // ToDo: Xunit runs tests in separate threads and this method may return before tests are finished
        };

        for (auto&& [i_class, i_method] : extra_functions)
        {
            if (defining_class->get_name() == i_class && method_name == i_method)
                return true;
        }
    }

    return false;
}
