module;
#include <Windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result_macros.h>
#include <future>
#include <optional>
#include <string>
export module shell;

export namespace shell
{
    // 调用 Windows 资源管理器执行命令，以避免在管理员模式下执行命令
    // 虽然 Windows 提供了 ShellExecute() 函数，然而它会以当前程序的权限执行命令
    // 假如日冕客户端是以管理员模式启动的，那么执行的命令本身也会有管理员的权限
    // 而我们实际上可能只是想要打开一个网站而已，没必要用上管理员的权限
    // 所以我们这边不使用 ShellExecute() 函数，而是获取 Windows 资源管理器的对象
    // Windows 资源管理器一般只会有用户权限，让替我们打开我们想要打开的文件或网站
    void open(std::wstring const& command);
    // 以管理员模式执行命令，除了无效命令等原因之外，
    // 假如用户拒绝以管理员模式执行，也将会抛出异常
    void run_as_administrator
    (
        std::wstring const& command,
        std::optional<std::wstring> arguments = std::nullopt
    );
}

module:private;

namespace
{
    struct thread_data
    {
        std::wstring const& command;
        std::promise<void> promise;
    };
    // 通过 thread_data 把参数传给 do_shell_open
    // 并把结果（或者异常）重新保存到 thread_data
    DWORD WINAPI shell_open_thread_wrapper(LPVOID parameter);
    // 调用 Windows 资源管理器执行命令，以避免在管理员模式下执行命令
    void do_shell_open(std::wstring const& command);
    // 获取 Windows 资源管理器的对象
    wil::com_ptr<IShellDispatch2> get_shell_application();
}

// 调用 Windows 资源管理器执行命令，以避免在管理员模式下执行命令
// 虽然 Windows 提供了 ShellExecute() 函数，然而它会以当前程序的权限执行命令
// 假如日冕客户端是以管理员模式启动的，那么执行的命令本身也会有管理员的权限
// 而我们实际上可能只是想要打开一个网站而已，没必要用上管理员的权限
// 所以我们这边不使用 ShellExecute() 函数，而是获取 Windows 资源管理器的对象
// Windows 资源管理器一般只会有用户权限，让替我们打开我们想要打开的文件或网站
void shell::open(std::wstring const& command)
{
    // 访问资源管理器需要通过 COM，为了避免我们的 COM 设置与 .NET 应用程序的
    // COM 设置产生冲突，我们使用 CreateThread 创建一个新的线程来执行这个操作。
    // https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex#remarks
    thread_data data{ .command = command };
    // 从 std::promise 获取 std::future，我们通过后者等待线程结束
    // 以及接收可能从线程中抛出的异常
    auto thread_result = data.promise.get_future();
    // 创建线程
    auto thread_handle = THROW_LAST_ERROR_IF_NULL(CreateThread
    (
        nullptr,
        0,
        shell_open_thread_wrapper,
        &data,
        0,
        nullptr
    ));
    // 我们其实并不需要访问线程对象，因此可以直接关闭它的句柄。
    CloseHandle(thread_handle);
    // 等待线程结束，或者接收异常
    thread_result.get();
}

void shell::run_as_administrator
(
    std::wstring const& command,
    std::optional<std::wstring> arguments
)
{
    // 调用 ShellExecute() 函数执行命令：
    // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutew
    auto result = ShellExecuteW
    (
        nullptr,
        L"runas",
        command.c_str(),
        arguments.has_value() ? arguments->c_str() : nullptr,
        nullptr,
        SW_SHOW
    );
    THROW_LAST_ERROR_IF_MSG(reinterpret_cast<std::intptr_t>(result) <= 32, "failed to run as administrator with ShellExecuteW");
}

namespace
{
    // 通过 thread_data 把参数传给 do_shell_open
    // 并把结果（或者异常）重新保存到 thread_data
    DWORD WINAPI shell_open_thread_wrapper(LPVOID parameter)
    {
        auto data = static_cast<thread_data*>(parameter);
        try
        {
            do_shell_open(data->command);
            // 通知函数已经执行完毕
            data->promise.set_value();
        }
        catch (...)
        {
            // 通知函数发送了异常
            data->promise.set_exception(std::current_exception());
        }
        return 0;
    }

    // 通过 Windos 资源管理器，以没有管理员权限的方式执行命令
    void do_shell_open(std::wstring const& command)
    {
        // 初始化 COM，并在函数结束的时候自动反初始化。
        // 注意：由于 .NET 的 GUI 程序本身也需要初始化 COM，
        // 本函数应当在新的线程里执行，以避免与 .NET 程序的 COM 设置冲突。
        // https://docs.microsoft.com/en-us/windows/win32/learnwin32/initializing-the-com-library
        // https://github.com/microsoft/wil/wiki/RAII-resource-wrappers#wilunique_call
        auto cleanup = wil::CoInitializeEx(COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        // 获取 Windows 资源管理器
        auto explorer = get_shell_application();
        // 使用 wil 提供的函数把字符串转换为 COM 使用的 BSTR 的类型：
        // BSTR 将会保存在智能指针里，以便自动释放
        // https://github.com/microsoft/wil/wiki/String-helpers#wilmake_something_string
        auto command_text = wil::make_bstr(command.c_str());
        VARIANT empty = {}; // VT_EMPTY
        // 让 Windows 资源管理器（以资源管理器的权限）执行命令
        // 使用 wil 的 THROW_IF_FAILED 来检查返回的 HRESULT
        THROW_IF_FAILED(explorer->ShellExecute
        (
            command_text.get(),
            empty,
            empty,
            empty,
            empty
        ));
    }

    wil::com_ptr<IShellDispatch2> get_shell_application()
    {
        // https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/winui/shell/appplatform/ExecInExplorer/ExecInExplorer.cpp
        // 创建对象，以便操作 Windows Shell（资源管理器）的窗口
        // https://docs.microsoft.com/en-us/windows/win32/api/exdisp/nn-exdisp-ishellwindows
        // 使用 wil 包装过的 CoCreateInstance 创建对象
        // https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cocreateinstance
        // https://github.com/microsoft/wil/wiki/WinRT-and-COM-wrappers#object-creation-helpers
        // wil 包装过的函数将会自动检查错误、并返回智能指针：
        // https://github.com/microsoft/wil/wiki/WinRT-and-COM-wrappers#wilcom_ptr_t
        auto shell_windows = wil::CoCreateInstance<IShellWindows>(CLSID_ShellWindows, CLSCTX_LOCAL_SERVER);
        // 找到代表桌面的窗口，并获取允许通过程序自动操作桌面的 IDispatch 接口：
        // https://docs.microsoft.com/en-us/windows/win32/api/exdisp/nf-exdisp-ishellwindows-findwindowsw
        // https://docs.microsoft.com/en-us/windows/win32/api/oaidl/nn-oaidl-idispatch
        HWND window = 0;
        VARIANT empty = {}; // VT_EMPTY
        wil::com_ptr<IDispatch> desktop_window_dispatch;
        // 使用 wil 提供的宏 THROW_IF_FAILED 来检查 FindWindowSW 返回的 HRESULT：
        // https://github.com/microsoft/wil/wiki/Error-handling-helpers#functions-returning-an-hresult
        THROW_IF_FAILED(shell_windows->FindWindowSW
        (
            &empty,
            &empty,
            SWC_DESKTOP,
            reinterpret_cast<long*>(&window),
            SWFO_NEEDDISPATCH,
            &desktop_window_dispatch
        ));
        // 我们找到的这个桌面窗口对象实际上实现了 IServiceProvider 接口
        // 尝试用 QueryInterface() 方法获取它：
        // https://docs.microsoft.com/en-us/cpp/atl/queryinterface?view=msvc-170
        // wil 的 com_ptr 提供了 query() 方法，它将会调用 QueryInterface()、
        // 检查错误、并返回新的智能指针。
        auto services = desktop_window_dispatch.query<IServiceProvider>();
        // 之后，通过 IServiceProvider 接口获取 IShellBrower 服务：
        // https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-ishellbrowser#remarks
        wil::com_ptr<IShellBrowser> shell_browser;
        THROW_IF_FAILED(services->QueryService
        (
            SID_STopLevelBrowser,
            IID_PPV_ARGS(&shell_browser)
        ));
        // 从桌面窗口的 IShellBrowser 服务获取当前启用的 IShellView
        wil::com_ptr<IShellView> shell_view;
        THROW_IF_FAILED(shell_browser->QueryActiveShellView(&shell_view));
        // 从 IShellView 获取能够操作文件夹或桌面背景界面的 IDispatch 接口：
        // https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ishellview-getitemobject
        // https://docs.microsoft.com/en-us/windows/win32/api/oaidl/nn-oaidl-idispatch
        wil::com_ptr<IDispatch> shell_background_dispatch;
        THROW_IF_FAILED(shell_view->GetItemObject
        (
            SVGIO_BACKGROUND,
            IID_PPV_ARGS(&shell_background_dispatch)
        ));
        // 把上面的 IDispatch 用 QueryInterface()
        // 转换为 IShellFolderViewDual，并通过后者获取 Shell 的应用程序对象：
        // https://docs.microsoft.com/en-us/windows/win32/api/shldisp/nn-shldisp-ishellfolderviewdual
        auto shell_folder_view_dual =
            shell_background_dispatch.query<IShellFolderViewDual>();
        wil::com_ptr<IDispatch> shell_application;
        THROW_IF_FAILED(shell_folder_view_dual->get_Application
        (
            &shell_application
        ));
        // 通过 QueryInterface() 获取应用程序对象的 IShellDispatch2 接口
        return shell_application.query<IShellDispatch2>();
    }
}
