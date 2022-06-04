// Date: 2022.6.3
// Author: Dynesshely
//
// Copyright © Dynesshely 2022-present
// All Rights Reserved.

#include "patch-runtime.h"

namespace PatchInstaller {

    arch GetSystemArchitecture() {
        SYSTEM_INFO stInfo;
        GetNativeSystemInfo(&stInfo);
        switch (stInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_INTEL:
            return arch::x86;
        case PROCESSOR_ARCHITECTURE_IA64:
            return arch::x64;
        case PROCESSOR_ARCHITECTURE_AMD64:
            return arch::x64;
        }
    }

    bool IsWin7() {
        typedef void(__stdcall* NTPROC)(DWORD*, DWORD*, DWORD*);
        HINSTANCE hinst = LoadLibrary(TEXT("ntdll.dll"));
        NTPROC GetNtVersionNumbers = (NTPROC)GetProcAddress(hinst, "RtlGetNtVersionNumbers");
        DWORD dwMajor, dwMinor, dwBuildNumber;
        GetNtVersionNumbers(&dwMajor, &dwMinor, &dwBuildNumber);

        if (dwMajor == 6 && dwMinor == 1)
            return true;
        else return false;
    }

    void DownloadPatch(arch arch) {
        switch (arch) {
        case arch::x64:
            NetHelper::DownloadToFile(patch_x64_link);
            break;
        case arch::x86:
            NetHelper::DownloadToFile(patch_x86_link);
            break;
        }
    }

    void DownloadRuntime() {
        NetHelper::DownloadToFile(runtime_x86_link);
    }

    void InstallPatch(arch arch) {
        switch (arch) {
        case arch::x64:
            system("Windows6.1-KB3063858-x64-v0.2.msu");
            system(cmd.c_str());
            break;
        case arch::x86:
            system("Windows6.1-KB3063858-x32-v0.2.msu");
            system(cmd.c_str());
            break;
        }
    }

    void InstallRuntime() {
        system("windowsdesktop-runtime-6.0.5-win-x86-v0.2.exe");
    }
}
