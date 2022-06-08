module;
#include <Windows.h>
#include <wil/resource.h>
#include <wil/result_macros.h>
#include <filesystem>
#include <future>
#include <optional>
#include <sstream>
#include <string>
export module process;
import text;

// 创建进程，并读取进程的输出
export class process
{
public:
    using line_reader = std::function<void(std::string_view)>;
    using stored_reader_pointer = line_reader (process::*); // 成员变量指针
private:
    wil::unique_handle m_job;
    std::future<void> m_read_stdout_task;
    std::future<void> m_read_stderr_task;
    line_reader m_output_reader;
    line_reader m_error_reader;

public:
    // 启动进程，它将返回一个可以用于等待进程结束的 std::future
    std::future<void> start
    (
        std::filesystem::path const& path,
        std::wstring arguments
    );
    // 设置函数，它将从另外一个线程逐行读取进程的标准输出
    void set_stdout_line_reader(line_reader reader);
    // 设置函数，它将从另外一个线程逐行读取进程的标准错误输出
    void set_stderr_line_reader(line_reader reader);
private:
    // 创建一个等待进程结束的异步任务
    static std::future<void> wait_for_process_exit
    (
        wil::unique_process_information process_information
    );
    // 创建一个逐行读取进程输出的异步任务
    std::future<void> start_read_process_output
    (
        wil::unique_handle pipe,
        stored_reader_pointer f
    );
};

module:private;

namespace
{
    struct inheritable_pipe // 可以被子进程继承的管道
    {
        wil::unique_handle read;
        wil::unique_handle write;
        inheritable_pipe(); // 创建可以被子进程继承的管道
    };
    void make_not_inheritable(HANDLE handle); // 禁止子进程继承
}

std::future<void> process::start
(
    std::filesystem::path const& path,
    std::wstring arguments
)
{
    FAIL_FAST_IF_MSG(m_job != nullptr, "process::start already called");
    // 创建一个新的 Job 对象，Job 对象可以控制进程的终止
    // 以确保之后创建的子进程不会比本进程活得更久
    {
        auto job = CreateJobObjectW(nullptr, nullptr);
        THROW_LAST_ERROR_IF_NULL_MSG(job, "CreateJobObjectW failed");
        m_job.reset(job);
    }
    // 创建能够被子进程使用的管道
    inheritable_pipe stdout_pipe;
    inheritable_pipe stderr_pipe;
    // 管道是有读取和写入的两端的，而我们这边使用的这一端并不需要能够被子进程使用
    make_not_inheritable(stdout_pipe.read.get());
    make_not_inheritable(stderr_pipe.read.get());
    // 子进程的启动信息
    STARTUPINFOW startup_information{ .cb = sizeof(STARTUPINFOW) };
    // 假如之前设置了读取子进程的输出的函数，
    // 就重定向子进程的标准输出到管道的写入端、通过管道来读取
    if (m_output_reader)
    {
        startup_information.dwFlags |= STARTF_USESTDHANDLES;
        startup_information.hStdOutput = stdout_pipe.write.get();
    }
    // 标准错误输出也一样
    if (m_error_reader)
    {
        startup_information.dwFlags |= STARTF_USESTDHANDLES;
        startup_information.hStdError = stderr_pipe.write.get();
    }
    // 修改命令行参数，让第一个参数（argv[0]）是进程的完整路径
    {
        std::wstringstream formatter;
        // std::filesystem::path 被输出到 stream 的时候，会自动加上引号
        // 所以我们就不用操心引号和空格的问题了（
        // https://en.cppreference.com/w/cpp/filesystem/path/operator_ltltgtgt
        formatter << std::filesystem::canonical(path) << L" " << arguments;
        // 更新字符串的内容
        arguments = formatter.str();
    }
    // 创建子进程
    wil::unique_process_information process_information;
    THROW_IF_WIN32_BOOL_FALSE(CreateProcessW
    (
        path.c_str(), // 要运行的程序的路径
        arguments.data(), // 子进程的参数
        nullptr, // 子进程的安全属性
        nullptr, // 子进程主线程的安全属性
        true, // 允许子进程继承管道
        0, // 不需要任何特殊的进程创建标志
        nullptr, // 默认环境变量
        nullptr, // 默认工作目录
        &startup_information, // 启动信息
        &process_information // 子进程的信息
    ));
    // 开始从另一个线程读取进程的标准输出
    m_read_stdout_task = start_read_process_output
    (
        std::move(stdout_pipe.read),
        &process::m_output_reader
    );
    // 开始从另一个线程读取进程的标准错误输出
    m_read_stderr_task = start_read_process_output
    (
        std::move(stderr_pipe.read),
        &process::m_error_reader
    );
    // 返回等待进程结束的异步任务
    return wait_for_process_exit(std::move(process_information));
}

// 设置函数，它将从另外一个线程逐行读取进程的标准输出
void process::set_stdout_line_reader(line_reader reader)
{
    // 只能在进程还没有启动的时候设置
    FAIL_FAST_IF_MSG(m_job != nullptr, "process::set_stdout_line_reader called after process::start");
    m_output_reader = reader;
}

// 设置函数，它将从另外一个线程逐行读取进程的标准错误输出
void process::set_stderr_line_reader(line_reader reader)
{
    // 只能在进程还没有启动的时候设置
    FAIL_FAST_IF_MSG(m_job != nullptr, "process::set_stderr_line_reader called after process::start");
    m_error_reader = std::move(reader);
}

// 创建一个等待进程结束的异步任务
std::future<void> process::wait_for_process_exit
(
    wil::unique_process_information process_information
)
{
    auto waiter = [p = std::move(process_information)]
    {
        auto wait_process_result = WaitForSingleObject(p.hProcess, INFINITE);
        THROW_LAST_ERROR_IF(wait_process_result != WAIT_OBJECT_0);
    };
    return std::async(std::launch::async, std::move(waiter));
}

// 创建一个逐行读取进程输出的异步任务
std::future<void> process::start_read_process_output
(
    wil::unique_handle pipe,
    stored_reader_pointer f
)
{
    // 假如 reader 不为 null，就调用它读取一行
    auto consume_line = [this, f](std::string_view line)
    {
        // text::try_split_one_line 按照 LF 分割字符串
        // 假如字符串使用 CRLF 作为结束符，那么字符串的尾部可能还遗留着 CR
        // 所以我们需要把 CR 去掉
        if (line.ends_with('\r'))
        {
            line.remove_suffix(1);
        }
        (this->*f)(line);
    };
    // 从管道读取子进程的输出
    return std::async(std::launch::async, [p = std::move(pipe), consume_line]
    {
        // 用于读取子进程的输出的缓冲区
        std::string buffer;
        // “上一次读取”时，读到一半的一行
        std::string partial_line;
        // 开始读取子进程的输出
        bool is_reading = true;
        while (is_reading)
        {
            buffer.resize(8192);
            DWORD bytes_read = 0;
            is_reading = ReadFile
            (
                p.get(), // 要读取的管道
                buffer.data(), // 缓冲区
                static_cast<DWORD>(buffer.size()), // 缓冲区大小
                &bytes_read, // 实际读取的字节数
                nullptr // 匿名管道不支持异步读取
            );
            auto last_error = GetLastError();
            if (not is_reading)
            {
                // 如果读取失败，则检查错误代码
                // 如果是错误代码为 ERROR_BROKEN_PIPE，则表示管道已经关闭
                // 否则就是其他意料之外的错误，抛出异常
                THROW_LAST_ERROR_IF(last_error != ERROR_BROKEN_PIPE);
            }
            // 把读取的数据分为两部分，当前的一行，以及剩下的部分
            buffer.resize(bytes_read);
            auto unparsed_part = std::string_view{ buffer };
            auto line = text::try_split_one_line(unparsed_part);
            while (line.has_value())
            {
                // 假如这次确实读完了一行，而上一次循环里最后一行没有没有读完
                // 就把这一次读取的数据拼接到上一次的最后一行
                if (not partial_line.empty())
                {
                    partial_line += line.value();
                    // 处理合并之后的一行
                    consume_line(partial_line);
                    // 表明上一次的最后一行已经被读完了
                    // 接下来就暂时不会再重新触发条件了
                    partial_line.clear();
                }
                else
                {
                    // 处理一行
                    consume_line(line.value());
                }
                // 读取下一行
                line = text::try_split_one_line(unparsed_part);
            }
            // 把没读完的部分放到 partial_line 中
            partial_line += unparsed_part;
        }
        // 处理最后剩下的那一行
        if (not partial_line.empty())
        {
            consume_line(partial_line);
        }
    });
}

namespace
{
    inheritable_pipe::inheritable_pipe() // 创建可以被子进程继承的管道
    {
        // https://docs.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output
        SECURITY_ATTRIBUTES security_attributes
        {
            .nLength = sizeof(SECURITY_ATTRIBUTES),
            .lpSecurityDescriptor = nullptr,
            .bInheritHandle = true, // 表明管道可以被子进程继承
        };
        // 创建管道
        HANDLE read_handle = nullptr;
        HANDLE write_handle = nullptr;
        // https://docs.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createpipe
        // 使用 wil 的 THROW_IF_WIN32_BOOL_FALSE 检测错误
        // https://github.com/microsoft/wil/wiki/Error-handling-helpers#win32-apis-returning-a-bool-result-where-getlasterror-must-be-called-on-failure
        THROW_IF_WIN32_BOOL_FALSE(CreatePipe
        (
            &read_handle,
            &write_handle,
            &security_attributes,
            0
        ));
        // 把管道句柄保存到智能指针
        read.reset(read_handle);
        write.reset(write_handle);
    }

    void make_not_inheritable(HANDLE handle) // 禁止子进程继承某个句柄
    {
        // 清除 HANDLE 的可继承属性
        // https://docs.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-sethandleinformation
        // 使用 wil 的 THROW_IF_WIN32_BOOL_FALSE 检测错误
        // https://github.com/microsoft/wil/wiki/Error-handling-helpers#win32-apis-returning-a-bool-result-where-getlasterror-must-be-called-on-failure
        THROW_IF_WIN32_BOOL_FALSE(SetHandleInformation
        (
            handle,
            HANDLE_FLAG_INHERIT,
            0
        ));
    }
}
