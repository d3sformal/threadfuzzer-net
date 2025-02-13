#include "cor_profiler_base.hpp"
#include "class_factory.hpp"

extern "C" void __stdcall functionEnterC(UINT_PTR function_ptr, COR_PRF_ELT_INFO elt_info)
{
    auto* function = profiler->get_function_info(function_ptr, elt_info);
    if (profiler->MethodEntry(function, elt_info) == S_OK)
        (void) 0;
}

extern "C" void __stdcall functionLeaveC(UINT_PTR function_ptr, COR_PRF_ELT_INFO elt_info)
{
    auto* function = profiler->get_function_info(function_ptr);
    if (profiler->MethodLeave(function, elt_info) == S_OK)
        (void) 0;
}

extern "C" HRESULT __stdcall stackSnapshotC(FunctionID func, UINT_PTR ip, COR_PRF_FRAME_INFO frame_info, ULONG32 ctx_size, BYTE context[], void* client_data)
{
    auto* function = func ? profiler->get_function_info(func) : nullptr;
    return profiler->StackSnapshot(function, ip, frame_info, client_data);
}

extern "C" UINT_PTR __stdcall functionIdMapper(FunctionID function_id, void*, BOOL* should_hook)
{
    *should_hook = profiler->should_hook(function_id);
    return function_id;
}
