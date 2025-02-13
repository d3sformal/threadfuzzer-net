#pragma once

#include "cor_profiler_base.hpp"
#include "cor_error_handling.hpp"
#include "net_types.hpp"
#include "thread_id.hpp"
#include "stack_info.hpp"
#include "thread_safe_logger.hpp"
#include "thread_interleaving_control/stop_points.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>

class cor_profiler : public cor_profiler_base
{
    logging_target log_target;
public:
    cor_profiler()
    {
        if (config_file::get_instance().empty())
            throw profiler_error(L"Config file is empty");

        int level = config_file::get_instance().get_value<int>(L"logging");
        const std::wstring& target = config_file::get_instance().get_value(L"logging_file");

        if (target == L"--")
            log_target = logging_target(level, std::wcerr);
        else if (target == L"-")
            log_target = logging_target(level, std::wcout);
        else if (!target.empty())
            log_target = logging_target(level, target);

        thread_mappings.reserve(512);
        classes.reserve(8192);
        functions.reserve(32 * 1024);

        for (const std::wstring& str : config_file::get_instance().get_values(L"weak_points"))
            stop_points.emplace_back(str);
        for (const std::wstring& str : config_file::get_instance().get_values(L"strong_points"))
            stop_points.emplace_back(str);
    }

    template<int Level, typename... Ts>
    void log(Ts&&... ts) const
    {
        log_target.print_line<Level>(std::forward<Ts>(ts)...);
    }

    template<int Level, typename... Ts>
    void log(std::pmr::memory_resource* mem_res, Ts&&... ts) const
    {
        log_target.print_line<Level>(mem_res, std::forward<Ts>(ts)...);
    }

    ~cor_profiler() override
    {
        log<logging_level::INFO>(L"Profiler exiting.");
    }

private:
    std::unordered_map<DWORD, thread_id> thread_mappings; // GetCurrentThreadId is key
    std::atomic<std::size_t> internal_id_counter;

    std::unordered_map<ClassID, std::unique_ptr<class_description>> classes;
    std::unordered_set<std::unique_ptr<class_description>> fake_classes;
    std::recursive_mutex cls_mutex;
    const class_description& register_class(ClassID class_id);

    std::unordered_map<FunctionID, std::unique_ptr<function_spec>> functions;
    std::recursive_mutex fun_mutex;
    const function_spec& register_function(FunctionID function_id, COR_PRF_ELT_INFO elt_info);

    bool is_value_type(ClassID class_id) const
    {
        ULONG32 unused;
        auto result = cor_profiler_info->GetBoxClassLayout(class_id, &unused);
        COR_ASSERT(result, profiler_error::H_ERROR_INVALID_ARGUMENT, S_OK);
        if (result == profiler_error::H_ERROR_INVALID_ARGUMENT)
            return false;
        if (result == S_OK)
            return true;

        throw profiler_error(L"unreached");
    }

    std::size_t get_class_size(ClassID class_id) const
    {
        if (is_value_type(class_id))
        {
            ULONG unused, byte_size;
            COR_SANITIZE(cor_profiler_info->GetClassLayout(class_id, nullptr, 0, &unused, &byte_size));
            return byte_size;
        }

        return class_description::POINTER_SIZE;
    }

    std::vector<argument_data> get_argument_data(const function_spec* function, COR_PRF_ELT_INFO elt_info) const;

    static std::wstring get_class_name(ClassID class_id, IMetaDataImport* metadata_import, mdTypeDef metadata);

public:
    const class_description* find_class(const std::wstring& name) const
    {
        if (auto iter = std::ranges::find_if(fake_classes, [&name](const auto& cls) { return cls->get_name_simple() == name; }); iter != fake_classes.end())
            return iter->get();
        if (auto iter = std::ranges::find_if(classes, [&name](const auto& kvp) { return kvp.second->get_name_simple() == name; }); iter != classes.end())
            return iter->second.get();
        if (auto iter = std::ranges::find_if(classes, [&name](const auto& kvp) { return kvp.second->get_name() == name; }); iter != classes.end())
            return iter->second.get();

        return nullptr;
    }

    const class_description& register_fake_class(std::unique_ptr<class_description>&& class_desc)
    {
        return **fake_classes.insert(std::move(class_desc)).first;
    }

    const class_description* get_class_info(ClassID class_id)
    {
        try
        {
            return &register_class(class_id);
        }
        COR_CATCH_HANDLER_VALUE(class_description::UNKNOWN)
    }

    std::wstring get_class_name_from_token(mdToken token, IMetaDataImport2* metadata_import);

    const function_spec* get_function_info(FunctionID function_id, COR_PRF_ELT_INFO elt_info = 0)
    {
        try
        {
            return &register_function(function_id, elt_info);
        }
        COR_CATCH_HANDLER_VALUE(function_spec::UNKNOWN)
    }

    const thread_id& get_thread_id(DWORD threadId) const
    {
        return thread_mappings.at(threadId);
    }

    stack_info get_stack_info(std::pmr::memory_resource* memory_resource, ThreadID thread = 0, std::size_t max_stack_size = 0) const
    {
        stack_info stack(memory_resource, max_stack_size);
        auto hresult = cor_profiler_info->DoStackSnapshot(thread, &stackSnapshotC, COR_PRF_SNAPSHOT_DEFAULT, &stack, nullptr, 0);
        COR_ASSERT(hresult, S_OK, CORPROF_E_STACKSNAPSHOT_UNSAFE, CORPROF_E_STACKSNAPSHOT_UNMANAGED_CTX, CORPROF_E_STACKSNAPSHOT_ABORTED);

        return stack;
    }

    bool should_hook(FunctionID function_id);

    bool is_entry_point(const function_spec* function) const
    {
        return is_entry_point(function->get_name(), function->get_defining_class()->get_name());
    }

    std::vector<stop_point> stop_points;

    bool is_entry_point(const std::wstring& method_name, const std::wstring& class_name) const
    {
        static stop_point::method_desc entry_point(config_file::get_instance().get_value(L"entry_point"));

        if (entry_point.matches(class_name, method_name))
            return true;

        if (class_name == L"Microsoft.VisualStudio.TestPlatform.MSTestFramework.TestMethodRunner" && method_name == L"ExecuteInternal")
            return true;

        if (class_name == L"NUnit.Framework.Internal.Reflect" && method_name == L"InvokeMethod")
            return true;

        return false;
    }

    struct current_exception
    {
        const class_description* thrown_type = nullptr;
        const function_spec* unwound_function = nullptr;
    } current_exception;

#define IMPL_BLANK { return S_OK; }  // NOLINT(cppcoreguidelines-macro-usage)

    HRESULT STDMETHODCALLTYPE ClassLoadFinished(ClassID classId, HRESULT hrStatus) override;
    HRESULT STDMETHODCALLTYPE ObjectAllocated(ObjectID objectId, ClassID classId) override;

    HRESULT STDMETHODCALLTYPE ThreadCreated(ThreadID threadId) override;
    HRESULT STDMETHODCALLTYPE ThreadAssignedToOSThread(ThreadID managedThreadId, DWORD osThreadId) override;
    HRESULT STDMETHODCALLTYPE ThreadNameChanged(ThreadID threadId, ULONG cchName, WCHAR name[]) override;

    HRESULT STDMETHODCALLTYPE ExceptionThrown(ObjectID thrown_object_id) override;
    HRESULT STDMETHODCALLTYPE ExceptionUnwindFunctionEnter(FunctionID functionId) override;
    HRESULT STDMETHODCALLTYPE ExceptionUnwindFunctionLeave() override;
    HRESULT STDMETHODCALLTYPE ExceptionCatcherLeave() override;

    HRESULT STDMETHODCALLTYPE ProfilerAttachComplete() override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ProfilerDetachSucceeded() override IMPL_BLANK

    HRESULT STDMETHODCALLTYPE AppDomainCreationStarted(AppDomainID appDomainId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE AppDomainCreationFinished(AppDomainID appDomainId, HRESULT hrStatus) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE AppDomainShutdownStarted(AppDomainID appDomainId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE AppDomainShutdownFinished(AppDomainID appDomainId, HRESULT hrStatus) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE AssemblyLoadStarted(AssemblyID assemblyId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE AssemblyLoadFinished(AssemblyID assemblyId, HRESULT hrStatus) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE AssemblyUnloadStarted(AssemblyID assemblyId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE AssemblyUnloadFinished(AssemblyID assemblyId, HRESULT hrStatus) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ModuleLoadStarted(ModuleID moduleId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ModuleLoadFinished(ModuleID moduleId, HRESULT hrStatus) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ModuleUnloadStarted(ModuleID moduleId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ModuleUnloadFinished(ModuleID moduleId, HRESULT hrStatus) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ModuleAttachedToAssembly(ModuleID moduleId, AssemblyID AssemblyId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ClassLoadStarted(ClassID classId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ClassUnloadStarted(ClassID classId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ClassUnloadFinished(ClassID classId, HRESULT hrStatus) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE FunctionUnloadStarted(FunctionID functionId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE JITCompilationStarted(FunctionID functionId, BOOL fIsSafeToBlock) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE JITCompilationFinished(FunctionID functionId, HRESULT hrStatus, BOOL fIsSafeToBlock) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE JITCachedFunctionSearchStarted(FunctionID functionId, BOOL* pbUseCachedFunction) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE JITCachedFunctionSearchFinished(FunctionID functionId, COR_PRF_JIT_CACHE result) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE JITFunctionPitched(FunctionID functionId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE JITInlining(FunctionID callerId, FunctionID calleeId, BOOL* pfShouldInline) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ThreadDestroyed(ThreadID threadId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RemotingClientInvocationStarted() override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RemotingClientSendingMessage(GUID* pCookie, BOOL fIsAsync) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RemotingClientReceivingReply(GUID* pCookie, BOOL fIsAsync) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RemotingClientInvocationFinished() override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RemotingServerReceivingMessage(GUID* pCookie, BOOL fIsAsync) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RemotingServerInvocationStarted() override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RemotingServerInvocationReturned() override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RemotingServerSendingReply(GUID* pCookie, BOOL fIsAsync) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE UnmanagedToManagedTransition(FunctionID functionId, COR_PRF_TRANSITION_REASON reason) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ManagedToUnmanagedTransition(FunctionID functionId, COR_PRF_TRANSITION_REASON reason) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RuntimeSuspendStarted(COR_PRF_SUSPEND_REASON suspendReason) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RuntimeSuspendFinished() override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RuntimeSuspendAborted() override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RuntimeResumeStarted() override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RuntimeResumeFinished() override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RuntimeThreadSuspended(ThreadID threadId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RuntimeThreadResumed(ThreadID threadId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE MovedReferences(ULONG cMovedObjectIDRanges, ObjectID oldObjectIDRangeStart[], ObjectID newObjectIDRangeStart[], ULONG cObjectIDRangeLength[]) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ObjectsAllocatedByClass(ULONG cClassCount, ClassID classIds[], ULONG cObjects[]) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ObjectReferences(ObjectID objectId, ClassID classId, ULONG cObjectRefs, ObjectID objectRefIds[]) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RootReferences(ULONG cRootRefs, ObjectID rootRefIds[]) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ExceptionSearchFunctionEnter(FunctionID functionId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ExceptionSearchFunctionLeave() override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ExceptionSearchFilterEnter(FunctionID functionId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ExceptionSearchFilterLeave() override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ExceptionSearchCatcherFound(FunctionID functionId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ExceptionOSHandlerEnter(UINT_PTR unused) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ExceptionOSHandlerLeave(UINT_PTR unused) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ExceptionUnwindFinallyEnter(FunctionID functionId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ExceptionUnwindFinallyLeave() override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ExceptionCatcherEnter(FunctionID functionId, ObjectID objectId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE COMClassicVTableCreated(ClassID wrappedClassId, REFGUID implementedIID, void* pVTable, ULONG cSlots) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE COMClassicVTableDestroyed(ClassID wrappedClassId, REFGUID implementedIID, void* pVTable) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ExceptionCLRCatcherFound() override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ExceptionCLRCatcherExecute() override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE GarbageCollectionStarted(int cGenerations, BOOL generationCollected[], COR_PRF_GC_REASON reason) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE SurvivingReferences(ULONG cSurvivingObjectIDRanges, ObjectID objectIDRangeStart[], ULONG cObjectIDRangeLength[]) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE GarbageCollectionFinished() override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE FinalizeableObjectQueued(DWORD finalizerFlags, ObjectID objectID) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE RootReferences2(ULONG cRootRefs, ObjectID rootRefIds[], COR_PRF_GC_ROOT_KIND rootKinds[], COR_PRF_GC_ROOT_FLAGS rootFlags[], UINT_PTR rootIds[]) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE HandleCreated(GCHandleID handleId, ObjectID initialObjectId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE HandleDestroyed(GCHandleID handleId) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE InitializeForAttach(IUnknown* pCorProfilerInfoUnk, void* pvClientData, UINT cbClientData) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ReJITCompilationStarted(FunctionID functionId, ReJITID rejitId, BOOL fIsSafeToBlock) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE GetReJITParameters(ModuleID moduleId, mdMethodDef methodId, ICorProfilerFunctionControl* pFunctionControl) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ReJITCompilationFinished(FunctionID functionId, ReJITID rejitId, HRESULT hrStatus, BOOL fIsSafeToBlock) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ReJITError(ModuleID moduleId, mdMethodDef methodId, FunctionID functionId, HRESULT hrStatus) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE MovedReferences2(ULONG cMovedObjectIDRanges, ObjectID oldObjectIDRangeStart[], ObjectID newObjectIDRangeStart[], SIZE_T cObjectIDRangeLength[]) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE SurvivingReferences2(ULONG cSurvivingObjectIDRanges, ObjectID objectIDRangeStart[], SIZE_T cObjectIDRangeLength[]) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ConditionalWeakTableElementReferences(ULONG cRootRefs, ObjectID keyRefIds[], ObjectID valueRefIds[], GCHandleID rootIds[]) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE GetAssemblyReferences(const WCHAR* wszAssemblyPath, ICorProfilerAssemblyReferenceProvider* pAsmRefProvider) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE ModuleInMemorySymbolsUpdated(ModuleID moduleId) override IMPL_BLANK

    HRESULT STDMETHODCALLTYPE DynamicMethodJITCompilationStarted(FunctionID functionId, BOOL fIsSafeToBlock, LPCBYTE ilHeader, ULONG cbILHeader) override IMPL_BLANK
    HRESULT STDMETHODCALLTYPE DynamicMethodJITCompilationFinished(FunctionID functionId, HRESULT hrStatus, BOOL fIsSafeToBlock) override IMPL_BLANK

    HRESULT MethodEntry(const function_spec* function, COR_PRF_ELT_INFO elt_info) const;
    HRESULT MethodLeave(const function_spec* function, COR_PRF_ELT_INFO elt_info) const;
    HRESULT StackSnapshot(const function_spec* function, UINT_PTR ip, COR_PRF_FRAME_INFO frame_info, void* client_data) const;

private:
    mutable std::function<void(const function_spec*, const std::vector<argument_data>&)> function_entry_hook;
    mutable std::function<void(const function_spec*)> function_leave_hook;

    UINT_PTR compute_native_offset(const function_spec* function, UINT_PTR ip, std::pmr::memory_resource* mem_resource) const;
    UINT_PTR compute_il_offset(const function_spec* function, UINT_PTR native_offset, std::pmr::memory_resource* mem_resource) const;

public:
    template<typename F>
    void set_function_entry_hook(F&& f) const
    {
        function_entry_hook = std::forward<F>(f);
    }

    template<typename F>
    void set_function_leave_hook(F&& f) const
    {
        function_leave_hook = std::forward<F>(f);
    }
};

#undef IMPL_BLANK
