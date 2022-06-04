// Date: 2022.6.3
// Author: Dynesshely
//
// Copyright © Dynesshely 2022-present
// All Rights Reserved.

#include <UrlMon.h>
#include <string>

#pragma comment(lib, "urlmon.lib")

namespace NetHelper {
    void DownloadToFile(const char* url);
}