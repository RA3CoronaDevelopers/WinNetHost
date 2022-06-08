// 系统库
#include <Windows.h>
#include <ShellApi.h>
// vcpkg
#include <wil/resource.h>
// 标准库
#include <cwchar>
#include <filesystem>
#include <format>
#include <span>
#include <string_view>

#include "patch-runtime.h"

// 模块
import hostfxr;
import error_handling;
import text;

// Windows 的 Main 函数
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    // 调用 error_handling.ixx 里的 initialize_error_handling
    // 初始化错误处理，然后用 execute_protected 执行代码
    // 这样程序崩溃的时候至少能看到个弹窗（
    initialize_error_handling();
    return execute_protected([]
    {
        // 使用 CommandLineToArgvW 分割命令行参数
        // CommandLineToArgvW 返回的内存需要调用 LocalFree 释放：
        // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-commandlinetoargvw#remarks
        // 而 wil::unique_hlocal_ptr 正好就是一种使用 LocalFree 来管理内存的类型：
        // https://github.com/Microsoft/wil/wiki/RAII-resource-wrappers#available-unique_any-simple-memory-patterns
        // 因此我们可以直接使用它
        auto argc = 0;
        wil::unique_hlocal_ptr<wchar_t*> argv;
        argv.reset(CommandLineToArgvW(GetCommandLineW(), &argc));
        // 通过 hostfxr.ixx 里的 hostfxr_dll 加载 DLL，
        // 加载成功之后再用 hostfxr_app_context 加载 .NET 应用
        try
        {
            hostfxr_dll::load();
        }
        catch (std::exception const& e)
        {
            // 错误信息
            auto message = text::format(L"HostFxr.dll 加载失败，可能是因为日冕客户端忘了安装 .NET 运行库\n{}", e.what());
            // 调用 error_handling.ixx 里的弹窗函数显示一个简陋的弹窗
            show_error_message_box(message.c_str());

            if (MessageBox(0, "是否需要日冕为您下载.NET运行时?", "日冕启动器", MB_YESNO) == 6) {
                PatchInstaller::DownloadRuntime();
                if (MessageBox(0, "运行时下载完毕, 是否为您安装?", "日冕启动器", MB_YESNO) == 6) {
                    PatchInstaller::InstallRuntime();
                }
                else {
                    MessageBox(0, "下载的.NET运行时安装包已经保存在日冕安装路径, 请手动安装", "日冕启动器", 0);
                }
            }
            else {
                MessageBox(0, "那么您可以手动下载.NET 6运行时并安装, 日冕启动器需要此运行时", "日冕启动器", 0);
            }
            return 1;
        }
        try
        {
            // 这里的 std::span 和 C# 的 Span 差不多是一回事
            // 总之，这里用 span 传递参数、启动 .NET 应用
            std::span arguments{ argv.get(), static_cast<std::size_t>(argc) };
            // TODO: 这里的写 CoronaLauncher.dll 是相对于【当前路径】的
            // 但是当前路径其实不一定就是本 EXE 所处的路径
            // 假如要更加可靠的话，可以这样：
            // #include <wil/stl.h>
            // #include <wil/win32_helpers.h>
            // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-queryfullprocessimagenamew
            // https://github.com/microsoft/wil/wiki/Win32-helpers#predefined-adapters-for-functions-that-return-variable-length-strings
            // auto this_exe = wil::QueryFullProcessImageNameW<std::wstring>();
            // std::filesystem::path result{ this_exe };
            // result.replace_filename("bin/CoronaLauncher.dll")
            // 然后把 result 作为参数传到 hostfxr_app_context::load() 里面
            hostfxr_app_context::load(arguments, "CoronaLauncher.dll");
        }
        catch (std::exception const& e)
        {
            // 错误信息
            auto message = text::format(L"CoronaLauncher.dll 加载失败，可能是因为日冕客户端的安装有问题\n也许是您的操作系统需要安装一个补丁\n{}", e.what());
            // 调用 error_handling.ixx 里的弹窗函数显示一个简陋的弹窗
            show_error_message_box(message.c_str());
            if (MessageBox(0, "是否需要日冕为您下载补丁?", "日冕启动器", MB_YESNO) == 6) {
                PatchInstaller::arch architecture = PatchInstaller::GetSystemArchitecture();
                PatchInstaller::DownloadPatch(architecture);
                if (MessageBox(0, "补丁下载完毕, 是否为您安装?", "日冕启动器", MB_YESNO) == 6) {
                    PatchInstaller::InstallPatch(architecture);
                }
                else {
                    MessageBox(0, "下载的补丁安装包已经保存在日冕安装路径, 请手动安装", "日冕启动器", 0);
                }
            }
            else {
                MessageBox(0, "那么您可以手动下载补丁并安装, .NET 运行时需要此补丁", "日冕启动器", 0);
            }
            return 1;
        }
        // 运行 .NET 应用
        // 其实，hostfxr_app_context::load() 也会返回自身的引用
        // 也就是其实可以这样：
        // hostfxr_app_context::load(...).run();
        // 不过，分开来的话，可以写更加细分的 try / catch
        // 也更加容易处理可能出现的报错（
        return hostfxr_app_context::get().run();
    });
}