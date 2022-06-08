// 这种以 .ixx 的后缀名结尾、内容里写着 "module" 的文件
// 就是 C++20 里新加的“模块”。据说是一种比头文件 / 源文件的组合更加高级的玩意（
// 开头的 "module;" 到下面的 "export module" 之间的区域被称为
// global module fragment，专门用来放传统的 #include 头文件
module;
// 系统库
#include <Windows.h>
// vcpkg
#include <coreclr_delegates.h>
#include <hostfxr.h>
#include <nethost.h>
#include <wil/result_macros.h>
#include <wil/stl.h>
#include <wil/win32_helpers.h>
// 标准库
#include <atomic>
#include <format>
#include <filesystem>
#include <functional>
#include <memory>
#include <source_location>
#include <span>
#include <vector>
// 辅助程序
#include "patch-runtime.h"
export module hostfxr;

// 抛出一个包含各种详细信息的异常，配合下面的 hostfxr_exception 使用
#define HOSTFXR_THROW(reason, source, type, ...) \
    throw type{ reason, std::source_location::current(), source, __VA_ARGS__ }

// 当 condition 的值为 false 的时候抛出异常，配合下面的 hostfxr_exception 使用
#define HOSTFXR_THROW_IF(condition, reason, type, ...) \
    ((condition) ? HOSTFXR_THROW(reason, #condition, type, __VA_ARGS__) : 0)

// 当 status 的值意味着 .NET Host 出错的时候，就抛出异常
// 配合下面的 hostfxr_error_result 使用
#define HOSTFXR_THROW_IF_FAILED(status, reason)                     \
    ([](int code) { if (code < 0)                                   \
    {                                                               \
        HOSTFXR_THROW(reason, #status, hostfxr_error_result, code); \
    }})(status)

// 用来代表处理 HostFxr 时发生的各种错误
export class hostfxr_exception : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error; // 引用基类的构造函数
    // 构造一个有着各种详细信息的对象，来代表当前发生的错误
    hostfxr_exception
    (
        std::string_view message, // 错误信息
        std::source_location location, // 当前错误在源码里的位置
        std::string_view source // 触发当前错误的源码原文
    );
    // 把各种错误信息变成能让人直接阅读的字符串
    static std::string format_message
    (
        std::string_view message, // 错误信息
        std::source_location location, // 当前错误在源码里的位置
        std::string_view source // 触发当前错误的源码原文
    );
};

// 带有 .NET Host 错误码的 hostfxr_exception
export class hostfxr_error_result : public hostfxr_exception
{
private:
    // https://github.com/dotnet/runtime/blob/main/docs/design/features/host-error-codes.md
    int m_status_code; // 错误码

public:
    // 构造一个有着各种详细信息的对象，来代表当前发生的错误
    hostfxr_error_result
    (
        std::string_view message, // 错误信息
        std::source_location location, // 当前错误在源码里的位置
        std::string_view source, // 触发当前错误的源码原文
        int code // .NET Host 错误码
    );
    int status_code() const noexcept; // 获取错误码
    // 把错误码变成能让人直接阅读的字符串
    static std::string format_status
    (
        std::string_view message, // 错误信息
        int code // .NET Host 错误码
    );
};

// 我们通过 HostFxr.dll 来寻找 .NET 框架
// 这个类用来负责 DLL 的加载
// 由于 DLL 只需要加载一次就够了，因此它会作为一个单例（
export class hostfxr_dll
{
private:
    // 使用 wil 提供的智能指针管理 DLL
    // https://github.com/microsoft/wil/wiki/RAII-resource-wrappers#available-unique_any-handle-types
    wil::unique_hmodule m_dll;

public:
    // 加载 HostFxr.dll 并获取单例的引用
    static hostfxr_dll const& load();
    // 从 HostFxr.dll 获取一个函数指针
    template<typename FunctionPointer>
    FunctionPointer get(char const* function_name) const;
private:
    hostfxr_dll(); // 加载 hostfxr.dll
    hostfxr_dll(hostfxr_dll&&) = delete; // 单例禁止拷贝或移动构造
    hostfxr_dll& operator=(hostfxr_dll&&) = delete; // 单例禁止赋值
};

// 作为下面的 std::unique_ptr 的删除器，释放 HostFxr 上下文的资源
// https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md#cleanup
class hostfxr_context_deleter
{
public:
    // std::unique_ptr 需要使用这个类型
    // https://en.cppreference.com/w/cpp/memory/unique_ptr#Member_types
    using pointer = hostfxr_handle;
private:
    hostfxr_close_fn m_close = nullptr;

public:
    // 保存用于释放 HostFxr 上下文的函数指针
    hostfxr_context_deleter(hostfxr_close_fn close) noexcept;
    // 由 std::unique_ptr 调用，释放 HostFxr 的上下文
    void operator()(hostfxr_handle context) const noexcept;
};
// 能够自动释放 hostfxr_context 的 std::unique_ptr
using unique_hostfxr_handle = std::unique_ptr<hostfxr_handle, hostfxr_context_deleter>;

// 下面这个玩意可以用来启动一个 .NET 的应用
// 它最多只能被使用一次，也就是，不能启动多个 .NET 应用
export class hostfxr_app_context
{
public:
    // 假如 .NET 应用还没被创建，
    // 调用 hostfxr_app_context::get() 时将会抛出这个异常
    struct singleton_not_created_yet : public hostfxr_exception
    {
        using hostfxr_exception::hostfxr_exception; // 继承构造函数
    };
private:
    // 当前 .NET 应用的 HostFxr 上下文
    unique_hostfxr_handle const m_handle;
    // 运行 .NET 应用的函数的函数指针
    // https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md#running-an-application
    std::atomic<hostfxr_run_app_fn> m_run_app = nullptr;
public:
    // 加载 .NET 应用
    static hostfxr_app_context& load
    (
        std::span<char_t const* const> arguments, // 命令行参数
        std::filesystem::path const& app_dll_path // .NET 应用程序集的路径
    );
    static hostfxr_app_context& get(); // 获取已经被加载的 .NET 应用
    int run(); // 运行 .NET 应用
    hostfxr_handle context_handle() const noexcept; // 获取 HostFxr 上下文
private:
    // 加载 .NET 应用的单例，或获取现有的 .NET 应用的单例
    static hostfxr_app_context& get_instance
    (
        std::span<char_t const* const> arguments, // 命令行参数
        std::filesystem::path app_dll_path, // .NET 应用程序集的路径
        bool get_existing_instance // 是否获取现有的单例（还是要创建新的？）
    );
    // 保存上下文的句柄以及执行应用的函数指针
    hostfxr_app_context
    (
        unique_hostfxr_handle handle,
        hostfxr_run_app_fn run_app
    ) noexcept;
};

// 下面这玩意可以用来加载任意的 .NET 程序集并调用里面的 static 函数
// 而且可以多次使用，也可以和 hostfxr_app_context 同时使用
// 看上去是很有用的（
// 不过，我们只需要让日冕客户端跑起来，
// 那样的话 host_fxr_app_context 已经够用了，而这个类就不是很有必要了。
// 可由于它看起来很有用，所以代码留下来当作参考（
export class hostfxr_component_context :
    std::enable_shared_from_this<hostfxr_component_context> // 支持智能指针
{
public:
    // 根据不同的平台，在 Windows 上使用 UTF-16 的 std::wstring / wchar_t
    // 在其他平台上使用 UTF-8 的 std::string / char
    using string_t = std::basic_string<char_t>;
private:
    // 当前 HostFxr 上下文
    unique_hostfxr_handle const m_handle;
    // 获取函数的函数的指针
    // https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md#loading-and-calling-managed-components
    load_assembly_and_get_function_pointer_fn m_load_and_get_function = nullptr;

public:
    // 根据 runtimeconfig.json 创建一个 hostfxr_component_context 上下文
    // 之后可以通过 get_method() 加载任意 .NET 程序集并调用里面的 .NET 函数
    static std::shared_ptr<hostfxr_component_context> new_context
    (
        std::filesystem::path runtime_config_path // runtimeconfig.json 的路径
    );
    // 使用当前现有的 .NET 应用的上下文（见 hostfxr_app_context）
    // 之后可以通过 get_method() 加载任意 .NET 程序集并调用里面的 .NET 函数
    // hostfxr_app_context::load() 必须已经被调用过，才能使用这个函数
    static std::shared_ptr<hostfxr_component_context> app_context();
    // 加载一个程序集，找到里面的 .NET 函数，并返回能够调用该函数的函数指针
    std::function<int(void*, int)> get_method
    (
        std::filesystem::path assembly_path, // .NET 程序集的路径
        string_t const& type_name, // 类型名
        string_t const& method_name // 方法名
    );
protected:
    // 使用智能指针创建 hostfxr_component_context
    static std::shared_ptr<hostfxr_component_context> create
    (
        unique_hostfxr_handle handle,
        hostfxr_get_runtime_delegate_fn get_delegate
    );
    // 保存上下文的句柄以及获取函数的函数指针
    hostfxr_component_context
    (
        unique_hostfxr_handle handle,
        hostfxr_get_runtime_delegate_fn get_delegate
    );
};

module:private;
// 函数实现

// 构造一个有着各种详细信息的对象，来代表当前发生的错误
hostfxr_exception::hostfxr_exception
(
    std::string_view message, // 错误信息
    std::source_location location, // 当前错误在源码里的位置
    std::string_view source // 触发当前错误的源码原文
) :
    // 调用基类构造函数
    std::runtime_error{ format_message(message, location, source) }
{}

// 把各种错误信息变成能让人直接阅读的字符串
std::string hostfxr_exception::format_message
(
    std::string_view message, // 错误信息
    std::source_location location, // 当前错误在源码里的位置
    std::string_view source // 触发当前错误的源码原文
)
{
    return std::format
    (
        "{}\n{}({},{}): {}:\n{}",
        message,
        location.file_name(),
        location.line(),
        location.column(),
        location.function_name(),
        source
    );
}

// 构造一个有着各种详细信息的对象，来代表当前发生的错误
hostfxr_error_result::hostfxr_error_result
(
    std::string_view message, // 错误信息
    std::source_location location, // 当前错误在源码里的位置
    std::string_view source, // 触发当前错误的源码原文
    int code // .NET Host 错误码
) :
    // 把参数传递给给基类构造函数
    hostfxr_exception{ format_status(message, code), location, source },
    m_status_code{ code } // 保存错误码
{}

// 获取错误码
int hostfxr_error_result::status_code() const noexcept
{
    return m_status_code;
}

// 把错误码变成能让人直接阅读的字符串
std::string hostfxr_error_result::format_status
(
    std::string_view message, // 错误信息
    int code // .NET Host 错误码
)
{
    auto link = "https://github.com/dotnet/runtime/blob/main/docs/design/features/host-error-codes.md";
    return std::format("{}\nhost error code 0x{:X}\n{}", message, static_cast<unsigned>(code), link);
}

// 加载 HostFxr.dll 并获取单例的引用
hostfxr_dll const& hostfxr_dll::load()
{
    static auto singleton = hostfxr_dll{};
    return singleton;
}

// 从 hostfxr.dll 获取一个函数指针
template<typename FunctionPointer>
FunctionPointer hostfxr_dll::get(char const* function_name) const
{
    // GetProcAddress 假如失败的话，会返回 null，错误码可以用 GetLastError() 获得：
    // https://docs.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getprocaddress
    auto function = GetProcAddress(m_dll.get(), function_name);
    // 使用 wil 提供的 THROW_LAST_ERROR_IF_NULL_MSG 来检查 null 并获取错误码：
    // https://github.com/microsoft/wil/wiki/Error-handling-helpers#report-the-result-of-getlasterror-based-upon-a-condition-or-null-check
    THROW_LAST_ERROR_IF_NULL_MSG(function, "failed to retrieve function %hs from hostfxr.dll", function_name);
    return reinterpret_cast<FunctionPointer>(function);
}

// 加载 hostfxr.dll
hostfxr_dll::hostfxr_dll()
{
    // 使用 nethost.h 提供的函数，寻找 hostfxr.dll 的路径
    // get_hostfxr_path 在提供的缓冲区不足以放下 hostfxr.dll 的完整路径的时候
    // 应当会返回 0x80008098 的错误码，并告诉我们 hostfxr.dll 路径的长度：
    // https://github.com/dotnet/runtime/blob/v6.0.5/src/native/corehost/nethost/nethost.h#L66
    std::vector<wchar_t> buffer;
    std::size_t size = 0;
    // 此时 get_hostfxr_path 必须返回 0x80008098
    // 毕竟大小为 0 的缓冲区绝对不可能放得下路径（
    // 使用 wil 的 FAIL_FAST_IF 来进行断言：
    // https://github.com/microsoft/wil/wiki/Error-handling-helpers#error-handling-techniques

    //FAIL_FAST_IF(get_hostfxr_path(buffer.data(), &size, nullptr) != 0x80008098);

    int result = get_hostfxr_path(buffer.data(), &size, nullptr);
    if (result != 0x80008098)
    {
        HOSTFXR_THROW_IF_FAILED(result, "我有足够的理由相信这个机子根本没有.NET运行时!");
    }

    // 准备了足够大的缓冲区之后，再次调用 get_hostfxr_path 获取 hostfxr.dll 的路径
    buffer.resize(size + 1);
    size = buffer.size();
    // 用我们在文件开头写的宏来检查 get_hostfxr_path 的返回值
    HOSTFXR_THROW_IF_FAILED(get_hostfxr_path(buffer.data(), &size, nullptr), "failed to retrieve hostfxr path");
    // 拿到了路径之后，就可以加载 DLL 了
    // 使用 LoadLibraryW 加载 HostFxr.dll
    // https://docs.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryw
    // 并把加载的 DLL 句柄保存在 wil 提供的智能指针里：
    // https://github.com/microsoft/wil/wiki/RAII-resource-wrappers#available-unique_any-handle-types
    m_dll.reset(LoadLibraryW(buffer.data()));
    // LoadLibraryW 假如失败的话，会返回 null，错误码可以用 GetLastError() 获得：
    // 使用 wil 的 THROW_LAST_ERROR_IF_NULL_MSG 来检查 null 并获取错误码：
    // https://github.com/microsoft/wil/wiki/Error-handling-helpers#report-the-result-of-getlasterror-based-upon-a-condition-or-null-check
    THROW_LAST_ERROR_IF_NULL_MSG(m_dll, "failed to load hostfxr.dll on path %ls", buffer.data());
}

// 保存用于释放 HostFxr 上下文的函数指针
hostfxr_context_deleter::hostfxr_context_deleter(hostfxr_close_fn close) noexcept :
    m_close{ close }
{}

// 由 std::unique_ptr 调用，释放 HostFxr 的上下文
void hostfxr_context_deleter::operator()(hostfxr_handle handle) const noexcept
{
    m_close(handle);
}

// 加载 .NET 应用
hostfxr_app_context& hostfxr_app_context::load
(
    std::span<char_t const* const> arguments, // 命令行参数
    std::filesystem::path const& app_dll_path // .NET 应用程序集的路径
)
{
    return get_instance(arguments, app_dll_path, false);
}

// 获取已经被加载的 .NET 应用
hostfxr_app_context& hostfxr_app_context::get()
{
    // 既然是已经被加载的现有应用，那么前两个参数就没有必要了
    // 直接用 {} 来传递默认值就行（
    return get_instance({}, {}, true);
}

// 运行 .NET 应用
int hostfxr_app_context::run()
{
    // .NET 应用只能运行一次，所以调用的时候就把函数指针设为 null
    auto run_app = m_run_app.exchange(nullptr);
    // 之后检查函数指针是不是已经被设为 null 了
    // 同样使用 wil 的 FAIL_FAST_IF_NULL_MSG 宏来进行断言
    FAIL_FAST_IF_NULL_MSG(run_app, "run_app is null, .NET application might have already runned");
    // 运行 .NET 应用，并在 .NET 应用结束之后，返回它的 Main() 函数的返回值
    // 假如 .NET 应用没有被成功加载，则返回负数作为错误码
    // 对于日冕客户端来说，它的 Main() 只会返回 0（正常）或者 1（错误），
    // 不会返回负数。所以，负数的错误码，和正数的返回值，是可以区分开来的。
    // 因此我们也可以在这里使用 HOSTFXR_THROW_IF_FAILED 来检查返回值，
    // 而不用担心日冕客户端的 Main() 函数返回奇怪的值扰乱了判断。
    int app_return_value = run_app(m_handle.get());
    HOSTFXR_THROW_IF_FAILED(app_return_value, "hostfxr_run_app failed");
    return app_return_value;
}

// 获取 .NET 应用上下文的句柄
hostfxr_handle hostfxr_app_context::context_handle() const noexcept
{
    return m_handle.get();
}

// 加载 .NET 应用的单例，或获取现有的 .NET 应用的单例
hostfxr_app_context& hostfxr_app_context::get_instance
(
    std::span<char_t const* const> arguments, // 命令行参数
    std::filesystem::path app_dll_path, // .NET 应用程序集的路径
    bool get_existing_instance // 是否获取现有的单例（还是要创建新的？）
)
{
    // 加载单例
    static auto& instance = ([&]() -> hostfxr_app_context&
    {
        // 函数内的 static 局部变量（上面的那个 instance）只会被初始化一次
        // 也就是说，目前这段代码只会在 instance 单例还没被初始化的时候被调用。
        // 假如这时 get_existing_instance 参数要求获取“现有”的单例，
        // 那肯定是有问题的，因为单例根本还没被创建呢（
        // 使用我们在文件开头写的宏，来检查这种情况并抛出异常
        HOSTFXR_THROW_IF(get_existing_instance, "trying to get existing hostfxr_app_context instance when it's not even created", singleton_not_created_yet);
        // 获取 .NET 应用的程序集的规范完整路径
        // https://en.cppreference.com/w/cpp/filesystem/canonical
        app_dll_path = std::filesystem::canonical(app_dll_path);
        // 获取当前 EXE 的完整路径
        // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-queryfullprocessimagenamew
        // https://github.com/microsoft/wil/wiki/Win32-helpers#predefined-adapters-for-functions-that-return-variable-length-strings
        auto this_exe = wil::QueryFullProcessImageNameW<std::wstring>();
        // 加载 HostFxr.dll
        auto& dll = hostfxr_dll::load();
        // 从 HostFxr.dll 获取初始化 .NET 应用的函数指针
        // https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md#initialize-host-context
        auto init_app = dll.get<hostfxr_initialize_for_dotnet_command_line_fn>("hostfxr_initialize_for_dotnet_command_line");
        // 以及运行应用的函数指针
        // https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md#running-an-application
        auto run_app = dll.get<hostfxr_run_app_fn>("hostfxr_run_app");
        // 事后释放 HostFxr 上下文的函数指针
        // https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md#cleanup
        auto close_context = dll.get<hostfxr_close_fn>("hostfxr_close");
        // 修正命令行参数，让第一个参数变成 .NET 程序集的完整路径
        std::vector<char_t const*> modified_arguments{ arguments.begin(), arguments.end() };
        // app_dll_path.c_str() 是直接获取 app_dll_path 内部的字符串（的指针）
        // 而 app_dll_path 作为函数参数，肯定会比 modified_arguments
        // 这个局部变量活得更长
        // 因此把 app_dll_path.c_str() 保存到 modified_arguments
        // 不会有生命周期的问题
        if (modified_arguments.empty())
        {
            modified_arguments.push_back(app_dll_path.c_str());
        }
        else
        {
            modified_arguments[0] = app_dll_path.c_str();
        }
        // 初始化 .NET 应用
        // https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md#running-app-with-additional-runtime-properties
        hostfxr_initialize_parameters init_parameters
        {
            .size = sizeof(init_parameters), // init_parameters 的大小
            .host_path = this_exe.c_str(), // 本 EXE 的路径
            .dotnet_root = nullptr // 让 HostFxr 自己去寻找 .NET 被装在了哪里
        };
        // 将要被创建的 .NET 应用的 HostFxr 上下文，使用智能指针进行管理
        unique_hostfxr_handle app_context_handle{ nullptr, close_context };
        // 调用 hostfxr_initialize_for_dotnet_command_line 执行初始化
        hostfxr_handle result = nullptr;
        // 用我们在文件开头写的宏来检查 hostfxr_initialize_for_dotnet_command_line 的返回值
        HOSTFXR_THROW_IF_FAILED(init_app
        (
            modified_arguments.size(),
            modified_arguments.data(),
            &init_parameters,
            &result
        ), "failed to initialize .NET application");
        // 把创建的 HostFxr 上下文保存到智能指针
        app_context_handle.reset(result);
        // 接下来就能初始化我们的 hostfxr_app_context 单例了（
        static auto singleton = hostfxr_app_context
        {
            std::move(app_context_handle),
            run_app
        };
        return singleton;
    })();
    return instance;
}

// 保存上下文的句柄以及执行应用的函数指针
hostfxr_app_context::hostfxr_app_context
(
    unique_hostfxr_handle handle,
    hostfxr_run_app_fn run_app
) noexcept :
    m_handle{ std::move(handle) }, // 保存 HostFxr 上下文
    m_run_app{ run_app } // 保存函数指针
{}

// 根据 runtimeconfig.json 创建一个 hostfxr_component_context 上下文
// 用于加载任意 .NET 程序集并调用它导出的 static 函数
std::shared_ptr<hostfxr_component_context> hostfxr_component_context::new_context
(
    std::filesystem::path runtime_config_path // runtimeconfig.json 的路径
)
{
    // 获取 runtimeconfig.json 的规范完整路径
    // https://en.cppreference.com/w/cpp/filesystem/canonical
    runtime_config_path = std::filesystem::canonical(runtime_config_path);
    // 加载 HostFxr.dll
    auto& dll = hostfxr_dll::load();
    // 从 HostFxr.dll 获取初始化 .NET 程序集的函数指针
    // https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md#initialize-host-context
    auto init_component = dll.get<hostfxr_initialize_for_runtime_config_fn>("hostfxr_initialize_for_runtime_config");
    // 以及用来从 .NET 运行时获取更多函数的函数指针
    // https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md#getting-a-delegate-for-runtime-functionality
    auto get_runtime_delegate = dll.get<hostfxr_get_runtime_delegate_fn>("hostfxr_get_runtime_delegate");
    // 事后释放 HostFxr 上下文的函数指针
    // https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md#cleanup
    auto close_context = dll.get<hostfxr_close_fn>("hostfxr_close");

    // 将要被创建的 .NET 组件上下文
    unique_hostfxr_handle component_context_handle{ nullptr, close_context };
    // 调用 hostfxr_initialize_for_runtime_config 执行初始化
    // https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md#getting-a-function-pointer-to-call-a-managed-method
    hostfxr_handle result = nullptr;
    // 用我们在文件开头写的宏来检查 hostfxr_initialize_for_runtime_config 的返回值
    HOSTFXR_THROW_IF_FAILED(init_component
    (
        runtime_config_path.c_str(),
        nullptr,
        &result
    ), "failed to initialize .NET component");
    // 等 .NET 上下文成功初始化之后，把它保存到智能指针里
    component_context_handle.reset(result);
    // 并创建 hostfxr_compnent_context
    return create(std::move(component_context_handle), get_runtime_delegate);
}

// 使用当前现有的 .NET 应用的上下文（见 hostfxr_app_context）
// 之后可以通过 get_method() 加载任意 .NET 程序集并调用里面的 .NET 函数
// hostfxr_app_context::load() 必须已经被调用过，才能使用这个函数
std::shared_ptr<hostfxr_component_context> hostfxr_component_context::app_context()
{
    // 从 HostFxr.dll 获取用来从 .NET 运行时获取更多函数的函数指针
    // https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md#getting-a-delegate-for-runtime-functionality
    auto& dll = hostfxr_dll::load();
    auto get_runtime_delegate = dll.get<hostfxr_get_runtime_delegate_fn>("hostfxr_get_runtime_delegate");
    // 使用为 null 的上下文句柄，来代表想要使用现有 .NET 应用的上下文
    // （参见 hostfxr_component_context 的构造函数）
    // 此外，让 unique_ptr 的删除器也使用 null 函数指针
    // 这是因为 unique_ptr 只有在不为 null 的时候才需要调用删除器去释放指针
    // 既然现在上下文为 null，那么删除器根本就不需要：
    // https://en.cppreference.com/w/cpp/memory/unique_ptr/~unique_ptr
    unique_hostfxr_handle null{ nullptr, nullptr };
    return create(std::move(null), get_runtime_delegate);
}

// 加载一个程序集，找到里面的 .NET 函数，并返回能够调用该函数的函数指针
std::function<int(void*, int)> hostfxr_component_context::get_method
(
    std::filesystem::path assembly_path, // .NET 程序集的路径
    string_t const& type_name, // 类型名
    string_t const& method_name // 方法名
)
{
    // 创建指向自身的智能指针
    // https://en.cppreference.com/w/cpp/memory/enable_shared_from_this/shared_from_this
    auto self = shared_from_this();

    // 获取 .NET 程序集的规范完整路径
    // https://en.cppreference.com/w/cpp/filesystem/canonical
    assembly_path = std::filesystem::canonical(assembly_path);
    // 加载 .NET 程序集、找到里面的函数、并返回能够调用 .NET 函数的函数指针
    // https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md#loading-and-calling-managed-components
    void* result = nullptr;
    // 用我们在文件开头写的宏来检查 load_assembly_and_get_function_pointer 的返回值
    HOSTFXR_THROW_IF_FAILED(m_load_and_get_function
    (
        assembly_path.c_str(), // 程序集路径
        type_name.c_str(), // 类型名称
        method_name.c_str(), // 方法名称
        nullptr, // 使用默认的函数签名
        nullptr, // 必须为 null 的保留参数
        &result
    ), "failed to load assembly and get function pointer");
    // 默认情况下拿到的 .NET 函数的函数签名是 component_entry_point_fn
    // 也就是 int (CORECLR_DELEGATE_CALLTYPE *)(void*, int32_t):
    // https://github.com/dotnet/runtime/blob/v6.0.5/src/native/corehost/coreclr_delegates.h#L35
    auto retrieved_function_pointer = static_cast<component_entry_point_fn>(result);
    // 创建并返回匿名函数
    // 这个匿名函数持有指向 hostfxr_component_context 的智能指针，
    // 也就是捕获列表里的“self”，以确保 hostfxr_component_context 的存活（
    return [self, retrieved_function_pointer](void* arg, int arg_size_in_bytes)
    {
        return retrieved_function_pointer(arg, arg_size_in_bytes);
    };
}

// 使用智能指针创建 hostfxr_component_context
std::shared_ptr<hostfxr_component_context> hostfxr_component_context::create
(
    unique_hostfxr_handle handle,
    hostfxr_get_runtime_delegate_fn get_delegate
)
{
    // hostfxr_component_context 的构造函数是私有的，
    // 这导致它无法通过 std::make_shared 创建
    // 但是可以通过下面这个类，来曲线救国，允许 std::make_shared（
    struct helper : hostfxr_component_context
    {
        helper(unique_hostfxr_handle x, hostfxr_get_runtime_delegate_fn y) :
            hostfxr_component_context{ std::move(x), y }
        {}
    };
    return std::make_shared<helper>(std::move(handle), get_delegate);
}

// 保存上下文的句柄以及获取函数的函数指针
hostfxr_component_context::hostfxr_component_context
(
    unique_hostfxr_handle handle,
    hostfxr_get_runtime_delegate_fn get_delegate
) :
    m_handle{ std::move(handle) } // 保存 .NET 上下文
{
    // 假如传入的 handle 为 null
    // 则表明是想要使用现有的 .NET 应用的上下文
    // （参见 app_context() 函数）
    hostfxr_handle raw_handle = m_handle != nullptr
        ? m_handle.get()
        : hostfxr_app_context::get().context_handle();

    // 通过调用 hostfxr_get_runtime_delegate 来获得一个函数指针
    // 得到这个函数指针之后，可以用它去【获取 .NET 函数的函数指针】
    // https://github.com/dotnet/runtime/blob/main/docs/design/features/native-hosting.md#getting-a-delegate-for-runtime-functionality
    void* result = nullptr;
    // 用我们在文件开头写的宏来检查 hostfxr_get_runtime_delegate 的返回值
    HOSTFXR_THROW_IF_FAILED(get_delegate
    (
        raw_handle,
        hostfxr_delegate_type::hdt_load_assembly_and_get_function_pointer,
        &result
    ), "failed to get runtime delegate of hdt_load_assembly_and_get_function_pointer");
    // 保存获得的函数指针
    m_load_and_get_function = static_cast<load_assembly_and_get_function_pointer_fn>(result);
}