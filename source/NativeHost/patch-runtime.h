// Date: 2022.6.3
// Author: Dynesshely
//
// Copyright © Dynesshely 2022-present
// All Rights Reserved.

#include <bits/stdc++.h>
#include <windows.h>
#include <sysinfoapi.h>
#include <versionhelpers.h>

#include "nethelper.h"

#define BASE_LINK "http://file.rmrts.com/srv/cor/redist/"

typedef __int32 i32;
typedef __int64 i64;

namespace PatchInstaller {

    const char* patch_x86_link = BASE_LINK "Windows6.1-KB3063858-x86-v0.2.msu";
    const char* patch_x64_link = BASE_LINK "Windows6.1-KB3063858-x64-v0.2.msu";
    const char* runtime_x64_link = BASE_LINK "windowsdesktop-runtime-6.0.5-win-x64-v0.2.exe";
    const char* runtime_x86_link = BASE_LINK "windowsdesktop-runtime-6.0.5-win-x86-v0.2.exe";

    enum arch {
        x86 = 0, x64 = 1
    };

    arch GetSystemArchitecture();

    bool IsWin7();

    void DownloadPatch(arch arch);

    void DownloadRuntime(arch arch);

    void InstallPatch(arch arch);

    void InstallRuntime(arch arch);
}




