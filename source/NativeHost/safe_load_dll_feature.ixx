module;
#include <Windows.h>
#include <wil/result_macros.h>
export module safe_load_dll_feature;

export namespace safe_load_dll_feature
{
    bool is_supported();
}

module:private;

bool safe_load_dll_feature::is_supported()
{
    auto kernel32_dll = GetModuleHandleW(L"kernel32.dll");
    THROW_LAST_ERROR_IF_NULL_MSG(kernel32_dll, "failed to retrieve kernel32.dll with GetModuleHandleW");
    // Detect if KB2533623 is installed
    // https://support.microsoft.com/en-us/topic/microsoft-security-advisory-insecure-library-loading-could-allow-remote-code-execution-486ea436-2d47-27e5-6cb9-26ab7230c704
    return GetProcAddress(kernel32_dll, "SetDefaultDllDirectories") != nullptr
        and GetProcAddress(kernel32_dll, "AddDllDirectory") != nullptr
        and GetProcAddress(kernel32_dll, "RemoveDllDirectory") != nullptr;
}
