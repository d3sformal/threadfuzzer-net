// Copyright (c) .NET Foundation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
// Modified

#include "class_factory.hpp"

BOOL STDMETHODCALLTYPE DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}

extern "C" HRESULT STDMETHODCALLTYPE DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID* ppv)
{
    if (ppv == nullptr)
        return E_FAIL;

    auto* factory = new class_factory();
    return factory->QueryInterface(riid, ppv);
}

extern "C" HRESULT STDMETHODCALLTYPE DllCanUnloadNow()
{
    return S_OK;
}
