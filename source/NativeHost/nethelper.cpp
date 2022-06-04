// Date: 2022.6.3
// Author: Dynesshely
//
// Copyright © Dynesshely 2022-present
// All Rights Reserved.

#include "nethelper.h"

const char* ARIA = "./tools/aria2c.exe";

namespace NetHelper {
    void DownloadToFile(const char* url) {
        std::string cmd("tools\\aria2c.exe ");
        cmd.append(url);
        system(cmd.c_str());
    }
}