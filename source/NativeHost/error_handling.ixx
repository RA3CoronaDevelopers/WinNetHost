// 这种以 .ixx 的后缀名结尾、内容里写着 "module" 的文件
// 就是 C++20 里新加的“模块”。据说是一种比头文件 / 源文件的组合更加高级的玩意（
// 开头的 "module;" 到下面的 "export module" 之间的区域被称为
// global module fragment，专门用来放传统的 #include 头文件
module;
#include <Windows.h>
#include <wil/result_macros.h>
#include <clocale>
#include <regex>
#include <string>
export module error_handling;
import text;

export // 本模块导出的函数
{
    // 初始化错误处理，主要是让程序能在崩溃之前，至少能显示一个弹窗
    void initialize_error_handling();
    // 显示一个简陋的报错弹窗
    void show_error_message_box(wchar_t const* message) noexcept;
    // 把 wil 的错误信息输出到字符串
    // https://github.com/microsoft/wil/wiki/Error-logging-and-observation#getfailurelogstring---printable-log-message
    std::wstring get_failure_description_text(wil::FailureInfo const& info);
    // 执行函数 F，尽量 catch 所有的错误、并在程序中止之前显示弹窗
    template<typename F>
    int execute_protected(F&& f)
    {
        int result = -1;
        // 除了普通的 C++ 异常以外，还有一些其他的错误，
        //（比如说解引用 null 指针、栈溢出这种），它们是无法被 catch 的，
        // 但是，wil 提供的这个 FailFastException 似乎能够把这种错误
        // 也给一起处理了，它将会在遇到错误的时候调用 failfast handler，
        // 报告错误（这一步可以由我们自定义）并终止程序。
        // 也就是说，可以根据我们在 initialize_error_handling 里的设置，
        // 显示报错弹窗。
        // TODO: 之前在 Win7 虚拟机测试的时候，
        // 程序崩溃的时候似乎不会被终止，而是死循环，需要调查一下
        wil::FailFastException(WI_DIAGNOSTICS_INFO, [&]()
        {
            try
            {
                // 尝试调用函数
                result = std::invoke(std::forward<F>(f));
            }
            // 假如抛出异常，则执行 wil failfast 的逻辑
            // 并根据我们在 initialize_error_handling 里的设置，显示弹窗
            CATCH_FAIL_FAST();
        });
        return result;
    }
}

module:private;

namespace // 供内部使用的函数
{
    // 用来在在程序崩溃的时候，根据 wil 的错误信息，显示报错弹窗
    void show_fatal_error_message_box(wil::FailureInfo const& info) noexcept;

    // 假如 C++ 遇到了无法处理的情况，std::terminate 就会被调用、并终止程序的运行
    // 但是，在这里我们也可以设置自定义的回调，std::terminate 就会调用我们的函数。
    // https://en.cppreference.com/w/cpp/error/set_terminate
    // 我们利用回调函数来触发一个 wil 的 failfast，
    // 这样就能直接复用下面 initialize_error_handling 设置的 failfast 错误处理、
    // 在程序崩溃之前显示弹窗了。
    // 比较坑的一点是，MSVC 的 std::set_terminate 只能对当前线程有效，
    // 所以只调用一次是不够的。
    // 不过，我们可以利用 thread_local 变量，在每个线程上都调用它
    thread_local auto result = std::set_terminate([]
    {
        try
        {
            // 重新抛出异常
            std::rethrow_exception(std::current_exception());
        }
        // 然后让 wil 的宏 catch 这个异常、直接触发 failfast
        // https://github.com/microsoft/wil/wiki/Error-handling-helpers#guard-macros-and-helpers
        CATCH_FAIL_FAST()
    });
}

// 初始化错误处理，主要是让程序能在崩溃之前，至少能显示一个弹窗
void initialize_error_handling()
{
    // 设置 C++ 的本地环境，让它使用系统当前的语言编码
    // 否则，在中文操作系统上，C++ 的异常的（中文）报错信息可能会出现乱码
    // https://en.cppreference.com/w/cpp/locale/setlocale
    std::setlocale(LC_ALL, ".ACP");
    // 使用 wil 进行断言、然而断言失败的时候，
    // wil 就会通过 fail fast （“快速失败”）函数中止程序的运行
    // https://github.com/microsoft/wil/wiki/Error-handling-helpers#fail-fast-based-error-handling
    // 此外，我们也将会把其他的程序无法正常处理的情况交给 wil 的 fail fast 函数
    // 让它帮忙中止程序的运行。
    // 在 wil 在中止程序之前，可以先执行一个由我们设置的回调函数。
    // 我们利用这个回调函数，来在程序崩溃之前显示一个弹窗。
    wil::g_pfnWilFailFast = [](wil::FailureInfo const& info) noexcept
    {
        show_fatal_error_message_box(info);
        return true;
    };
}

// 显示一个简陋的报错弹窗
void show_error_message_box(wchar_t const* message) noexcept
{
    // 从 C++20 开始 string 的默认构造函数同样也是 noexcept
    // 也就是，声明一个空的 string 必定不会触发异常
    // https://en.cppreference.com/w/cpp/string/basic_string/basic_string
    std::wstring buffer;
    try
    {
        // Windows 的 MessageBox 要使用 \r\n 作为换行符
        // 所以，尝试把所有的 \n 都替换成 \r\n
        buffer = text::to_crlf(message);
        message = buffer.c_str();
    }
    // 假如替换换行符的时候发生了异常，那也就算了，把异常忽略掉假装无事发生（
    // 毕竟，哪怕没换行，又不是不能看（（
    catch (...) {}
    // 弹窗会有个 OK 按钮，会显示在其他窗口的上面，还会有个红色感叹号
    auto flags = MB_OK | MB_ICONERROR | MB_SETFOREGROUND;
    // 显示弹窗
    MessageBoxW(nullptr, message, L"CoronaLauncher", flags);
}

// 把 wil 的错误信息输出到字符串
// https://github.com/microsoft/wil/wiki/Error-logging-and-observation#getfailurelogstring---printable-log-message
std::wstring get_failure_description_text(wil::FailureInfo const& info)
{
    std::wstring buffer;
    buffer.resize(4096);
    // wil::GetFailureLogString 返回 HRESULT 错误码
    // 使用 Windows.h 提供的 SUCCEEDED() 宏来检查 HRESULT 的值
    if (SUCCEEDED(wil::GetFailureLogString(buffer.data(), buffer.size(), info)))
    {
        return buffer;
    }
    return L"(failed to retrieve error message)";
}

namespace
{
    // 用来在在程序崩溃的时候，根据 wil 的错误信息，显示报错弹窗
    void show_fatal_error_message_box(wil::FailureInfo const& info) noexcept
    {
        // 从 C++20 开始 string 的默认构造函数同样也是 noexcept
        // 也就是，声明一个空的 string 必定不会触发异常
        // https://en.cppreference.com/w/cpp/string/basic_string/basic_string
        std::wstring buffer;
        // 默认的错误信息
        auto message = L"(failed to retrieve error message)";
        try
        {
            // 尝试把真正的错误信息变成字符串
            buffer = get_failure_description_text(info);
            // 假如成功了，就让 message 指针指向 buffer 里新的字符串
            message = buffer.c_str();
        }
        // 假如失败了，那么 message 里依然是默认的错误信息
        catch (...) {}
        // 显示弹窗
        show_error_message_box(message);
    }
}
