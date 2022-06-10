module;
#include <Windows.h>
#include <CommCtrl.h>
#include <wil/resource.h>
#include <wil/result_macros.h>
#include <wil/win32_helpers.h>
#include <functional>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <vector>
#include "resource.h"
export module gui;
import error_handling;
import shell;
import text;

namespace
{
    class progress_data;
}

export namespace gui
{
    class progress_control;
    enum class information_buttons_type
    {
        ok,
        yes_no,
    };

    // 显示一个已经在资源文件里定义好的对话框，
    // 并根据用户在对话框里按了哪个按钮来返回一个值
    // 通常来说，按下“确定”或“是”会返回 true，否则返回 false。
    // 假如之前已经在显示另一个窗口了，那么建议把现有窗口的句柄传进来作为本函数的
    // owner_window 参数，这样就可以让新的对话框在 owner_window 的上面显示。
    // 假如没有现有的窗口，那么 owner_window 可以是 nullptr
    bool show_information(HWND owner_window, int gui_resource_id);

    // 显示一个自定义文本以及按钮类型的对话框
    // 并根据用户在对话框里按了哪个按钮来返回一个值
    // 通常来说，按下“确定”或“是”会返回 true，否则返回 false。
    // 对话框文本里可以包含 <A HREF="..."></A> 形式（可能确实要大写）的超链接，
    // 具体可以参考微软 SysLink 控件的文档：
    // https://docs.microsoft.com/en-us/windows/win32/controls/syslink-overview#syslink-markup
    // 假如之前已经在显示另一个窗口了，那么建议把现有窗口的句柄传进来作为本函数的
    // owner_window 参数，这样就可以让新的对话框在 owner_window 的上面显示。
    // 假如没有现有的窗口，那么 owner_window 可以是 nullptr
    bool show_information
    (
        HWND owner_window,
        std::wstring const& message,
        std::wstring const& title,
        information_buttons_type buttons_type
    );

    // 显示一个自定义文本以及按钮类型的对话框
    // 并在用户按下按钮后调用一个回调函数
    // 回调函数的参数是当前窗口的句柄、以及用户按下的按钮 ID
    // 按钮 ID 可以在资源文件里找到，一般是 IDOK 或 IDYES 之类的
    // 可以在回调函数内部调用 EndDialog() 来关闭对话框：
    // https://docs.microsoft.com/en-us/windows/desktop/api/winuser/nf-winuser-enddialog
    // 也可以让回调函数返回 false 来执行按钮对应的默认操作。
    // 注意：假如用户点击窗口右上角的关闭按钮，那么回调函数不会被调用
    // 对话框文本里可以包含 <A HREF="..."></A> 形式（可能确实要大写）的超链接，
    // 具体可以参考微软 SysLink 控件的文档：
    // https://docs.microsoft.com/en-us/windows/win32/controls/syslink-overview#syslink-markup
    // 假如之前已经在显示另一个窗口了，那么建议把现有窗口的句柄传进来作为本函数的
    // owner_window 参数，这样就可以让新的对话框在 owner_window 的上面显示。
    // 假如没有现有的窗口，那么 owner_window 可以是 nullptr
    void show_information
    (
        HWND owner_window,
        std::wstring const& title,
        std::wstring const& message,
        information_buttons_type buttons_type,
        std::function<bool(HWND, int button_id)> on_click
    );

    // 显示一个能够用来展示下载进度的对话框
    // 对话框被创建之后，将会通过回调函数提供一个 progress_control 对象，
    // 用来控制对话框的内容和行为。
    // 该对象的所有 public 方法都是线程安全的，
    // 所有的操作都将会被排队到一个队列里，然后在主线程里依次执行。
    // 可以从别的线程调用该对象的 public 方法来更新对话框的进度。
    // 假如之前已经在显示另一个窗口了，那么建议把现有窗口的句柄传进来作为本函数的
    // owner_window 参数，这样就可以让新的对话框在 owner_window 的上面显示。
    // 假如没有现有的窗口，那么 owner_window 可以是 nullptr
    void show_download
    (
        HWND owner_window,
        int gui_resource_id,
        std::function<void(progress_control)> provide_control
    );
    // 用来控制 show_download() 创建的对话框的对象
    class progress_control
    {
    private:
        std::shared_ptr<progress_data> m_data;

    public:
        progress_control(std::shared_ptr<progress_data> data);
        // 设置处理按钮点击事件的函数，函数参数为当前窗口的句柄以及按钮的 id
        // 该函数返回 true 表明已经自行处理了事件，返回 false 则执行默认操作
        void set_button_handler
        (
            std::function<bool(HWND, int button_id)> on_click
        ) const;
        // 设置处理窗口关闭事件的函数，函数参数是当前窗口的句柄，
        // 该函数返回 true 代表允许关闭窗口
        void set_close_window_handler
        (
            std::function<bool(HWND)> allow_close_window
        ) const;
        // 设置进度条的进度
        void set_progress(int percentage) const;
        // 把进度条改为没有特定进度的滚动模式
        void set_indeterminate_progress() const;
        // 设置状态文本
        void set_status_text(std::wstring const& text) const;
        // 假如窗口仍然存在，就尝试在 UI 线程上执行指定的操作。
        // 将会获得当前窗口的句柄作为参数
        void run_on_ui_thread(std::function<void(HWND)> custom) const;
        // 主动关闭当前窗口，将会导致 close_window_handler 被执行
        void stop() const;
        // 检测窗口是否仍然存在
        bool window_exist() const;
    };
}

module:private; // 内部实现

namespace
{
    // 控制常规 DialogBox 窗口
    class window
    {
    public:
        enum class set_style_mode { clear, set };
    protected:
        mutable std::recursive_mutex m_mutex;
        std::queue<std::function<void(HWND)>> m_queued_actions;
        std::function<bool(HWND, int button_id)> m_on_button_click;
        wil::unique_hfont m_font;
        HWND m_window = nullptr;
        bool m_result = false;

    public:
        virtual ~window() = default;
        // 在窗口初始化之后，执行一个函数
        void execute_on_initialization(std::function<void(HWND)> action);
        // 假如窗口还没被销毁，则尝试在 UI 线程上执行一个函数
        void execute_on_ui_thread(std::function<void()> action);
        // 假如窗口还没被销毁，则尝试在 UI 线程上执行一个函数，
        // 并接受当前窗口的句柄作为参数。
        void execute_on_ui_thread(std::function<void(HWND)> action);
        // 设置处理按钮点击事件的函数，函数参数为当前窗口的句柄以及按钮的 id
        // 该函数返回 true 表明已经自行处理了事件，返回 false 则执行默认操作
        void set_button_handler
        (
            std::function<bool(HWND, int button_id)> on_click
        );
        // 处理 UI 事件
        // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-dlgproc
        static INT_PTR __stdcall process_event
        (
            HWND target,
            UINT message,
            WPARAM arg_1,
            LPARAM arg_2
        ) noexcept;
        // 设置窗口/子窗口的样式
        static void set_style(HWND item, int style, set_style_mode how);
        // 检测窗口的 Win32 类型名称
        static bool test_class_name(HWND target, std::wstring_view name);
        // 检测当前 WM_COMMAND 消息是否代表着着发生了点击按钮的事件
        static bool is_button_click_message(WPARAM arg_1, LPARAM arg_2);
        // 检测当前 WM_NOTIFY 消息是否代表着着发生了点击超链接的事件
        static bool is_link_notification(LPARAM notification_data);
    protected:
        virtual bool on_initialization(HWND target);
        virtual bool try_execute_queued_functions();
        virtual bool try_process_button_click(WPARAM arg_1);
        virtual bool try_process_link_notification(LPARAM link_data);
        virtual bool try_process_close();
    };

    // 控制显示下载进度的窗口
    class progress_data : public window
    {
    private:
        std::function<bool(HWND)> m_allow_close_window;

    public:
        // 设置处理窗口关闭事件的函数，函数参数是当前窗口的句柄，
        // 该函数返回 true 代表允许关闭窗口
        void set_close_window_handler
        (
            std::function<bool(HWND)> allow_close_window
        );
        void set_progress(int percentage);
        void start_indeterminate_progress();
        void stop_indeterminate_progress();
        void set_status_text(std::wstring const& text);
        void stop();
        // 在窗口关闭之后调用该函数清理资源。
        // 队列中可能保存了各种各样的函数对象，假如它们恰好又有引用计数的智能指针
        // 就有可能出现智能指针循环引用、导致内存泄漏的情况。
        // 所以，需要手动调用该函数来清空队列。
        void clear_queued_actions();
        // 检测窗口是否依然存在，这个函数是线程安全的
        bool window_exist() const;
    protected:
        bool try_process_close() override;
    };
}

bool gui::show_information(HWND owner_window, int gui_resource_id)
{
    window data;
    auto result = DialogBoxParamW
    (
        nullptr,
        MAKEINTRESOURCEW(gui_resource_id),
        owner_window,
        &data.process_event,
        reinterpret_cast<LPARAM>(&data)
    );
    // DialogBoxParamW 在失败的时候会返回 -1：
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-dialogboxparamw#return-value
    THROW_LAST_ERROR_IF_MSG(result == -1, "DialogBoxParamW failed");
    return result;
}

bool gui::show_information
(
    HWND owner_window,
    std::wstring const& message,
    std::wstring const& title,
    information_buttons_type buttons_type
)
{
    window data;
    // 在窗口初始化之后设置窗口标题与文本
    // 这里的 lambda 函数一定会在本函数结束之前被调用
    // 因此通过引用捕获 message 和 title 并不会导致生命周期方面的问题
    data.execute_on_initialization([&message, &title](HWND target)
    {
        SetWindowTextW(target, title.c_str());
        SetDlgItemTextW(target, IDC_MESSAGE, message.c_str());
    });
    // 根据传递的按钮类型，加载不同的窗口资源
    int gui_resource_id = buttons_type == information_buttons_type::ok
        ? IDD_INFORMATION_OK
        : IDD_INFORMATION_YESNO;
    auto result = DialogBoxParamW
    (
        nullptr,
        MAKEINTRESOURCEW(gui_resource_id),
        owner_window,
        &data.process_event,
        reinterpret_cast<LPARAM>(&data)
    );
    // DialogBoxParamW 在失败的时候会返回 -1：
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-dialogboxparamw#return-value
    THROW_LAST_ERROR_IF_MSG(result == -1, "DialogBoxParamW failed");
    return result;
}

void gui::show_information
(
    HWND owner_window,
    std::wstring const& message,
    std::wstring const& title,
    information_buttons_type buttons_type,
    std::function<bool(HWND, int button_id)> on_click
)
{
    window data;
    // 设置处理按钮点击事件的函数
    data.set_button_handler(std::move(on_click));
    // 在窗口初始化之后设置窗口标题与文本
    // 这里的 lambda 函数一定会在本函数结束之前被调用
    // 因此通过引用捕获 message 和 title 并不会导致生命周期方面的问题
    data.execute_on_initialization([&message, &title](HWND target)
    {
        SetWindowTextW(target, title.c_str());
        SetDlgItemTextW(target, IDC_MESSAGE, message.c_str());
    });
    // 根据传递的按钮类型，加载不同的窗口资源
    int gui_resource_id = buttons_type == information_buttons_type::ok
        ? IDD_INFORMATION_OK
        : IDD_INFORMATION_YESNO;
    THROW_LAST_ERROR_IF_MSG(DialogBoxParamW
    (
        nullptr,
        MAKEINTRESOURCEW(gui_resource_id),
        owner_window,
        &data.process_event,
        reinterpret_cast<LPARAM>(&data)
    ) == -1, "DialogBoxParamW failed");
    // DialogBoxParamW 在失败的时候会返回 -1：
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-dialogboxparamw#return-value
}

void gui::show_download
(
    HWND owner_window,
    int gui_resource_id,
    std::function<void(gui::progress_control)> provide_control
)
{
    auto data = std::make_shared<progress_data>();
    // 使用 wil 的 scope_exit 对象来自动在本函数结束的时候
    // 清理可能遗留在队列中的函数对象
    // https://github.com/microsoft/wil/wiki/RAII-resource-wrappers#wilscope_exit
    auto cleanup = wil::scope_exit([&data] { data->clear_queued_actions(); });
    // 在窗口初始化之后调用 provide_control 回调函数，传递 progress_control 对象
    data->execute_on_initialization(std::bind
    (
        std::move(provide_control),
        gui::progress_control{ data }
    ));
    THROW_LAST_ERROR_IF_MSG(DialogBoxParamW
    (
        nullptr,
        MAKEINTRESOURCEW(gui_resource_id),
        owner_window,
        &data->process_event,
        reinterpret_cast<LPARAM>(data.get())
    ) == -1, "DialogBoxParamW failed");
    // DialogBoxParamW 在失败的时候会返回 -1：
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-dialogboxparamw#return-value
}

gui::progress_control::progress_control(std::shared_ptr<progress_data> data) :
    m_data{ std::move(data) }
{}

void gui::progress_control::set_close_window_handler
(
    std::function<bool(HWND)> allow_close_window
) const
{
    auto action = [data = m_data, p = std::move(allow_close_window)]() mutable
    {
        data->set_close_window_handler(std::move(p));
    };
    m_data->execute_on_ui_thread(std::move(action));
}

void gui::progress_control::set_progress(int percentage) const
{
    m_data->execute_on_ui_thread([data = m_data, percentage]
    {
        data->set_progress(percentage);
    });
}

void gui::progress_control::set_indeterminate_progress() const
{
    m_data->execute_on_ui_thread([data = m_data]
    {
        data->start_indeterminate_progress();
    });
}

void gui::progress_control::set_status_text(std::wstring const& text) const
{
    m_data->execute_on_ui_thread([data = m_data, text]
    {
        data->set_status_text(text);
    });
}

void gui::progress_control::stop() const
{
    m_data->execute_on_ui_thread([data = m_data] { data->stop(); });
}

bool gui::progress_control::window_exist() const
{
    return m_data->window_exist();
}

// 在窗口初始化之后，执行一个函数
void window::execute_on_initialization(std::function<void(HWND)> action)
{
    // 确保是在窗口初始化之前调用的
    FAIL_FAST_IF_MSG(m_window != nullptr, "window already initialized");
    // 将函数添加到队列中，等待窗口初始化
    // 当窗口初始化时，会通过 on_initialization 调用所有添加的函数
    m_queued_actions.push(std::move(action));
}

// 假如窗口还没被销毁，则尝试在 UI 线程上执行一个函数
void window::execute_on_ui_thread(std::function<void()> action)
{
    execute_on_ui_thread([f = std::move(action)](HWND) { f(); });
}

// 假如窗口还没被销毁，则尝试在 UI 线程上执行一个函数，并接受当前窗口的句柄作为参数
// 该函数能从任意线程调用
void window::execute_on_ui_thread(std::function<void(HWND)> action)
{
    // 这个函数有可能是从别的线程调用的，那么访问 UI 线程之前自然要上锁
    std::scoped_lock lock{ m_mutex };
    // 假如 UI 窗口已经被销毁，那就不执行了
    if (m_window == nullptr)
    {
        return;
    }
    // 把需要执行的函数放到队列里
    m_queued_actions.push(std::move(action));
    // 然后通知 UI 线程的 event_loop，表明有新的函数需要执行了
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postmessagew
    // https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-app
    PostMessageW(m_window, WM_APP, 0, 0);
}

// 设置处理按钮点击事件的函数，函数参数为当前窗口的句柄以及按钮的 id
// 该函数返回 true 表明已经自行处理了事件，返回 false 则执行默认操作
void window::set_button_handler(std::function<bool(HWND, int button_id)> on_click)
{
    m_on_button_click = std::move(on_click);
}

// 处理 UI 事件
// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-dlgproc
INT_PTR __stdcall window::process_event
(
    HWND target,
    UINT message,
    WPARAM arg_1,
    LPARAM arg_2
) noexcept
{
    // 获取自定义数据
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowlongptrw
    auto user_data = GetWindowLongPtrW(target, GWLP_USERDATA);
    auto self = reinterpret_cast<window*>(user_data);
    try
    {
        switch (message)
        {
        case WM_INITDIALOG: // 初始化 - https://docs.microsoft.com/en-us/windows/win32/dlgbox/wm-initdialog
            // 初始化用户数据
            // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowlongptrw
            SetWindowLongPtrW(target, GWLP_USERDATA, arg_2);
            self = reinterpret_cast<window*>(arg_2);
            return self->on_initialization(target);
        case WM_APP: // 自定义事件 - https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-app
            return self->try_execute_queued_functions(); // 执行队列里的函数
        case WM_COMMAND: // 控件事件通知 - https://docs.microsoft.com/en-us/windows/win32/menurc/wm-command
            // 按钮事件通知
            // https://docs.microsoft.com/en-us/windows/win32/controls/bn-clicked
            if (is_button_click_message(arg_1, arg_2))
            {
                return self->try_process_button_click(arg_1);
            }
            break;
        case WM_NOTIFY: // 控件事件通知 - https://docs.microsoft.com/en-us/windows/win32/controls/wm-notify
            // 超链接事件通知
            // https://docs.microsoft.com/en-us/windows/win32/controls/nm-click-syslink
            if (is_link_notification(arg_2))
            {
                return self->try_process_link_notification(arg_2);
            }
            break;
        case WM_CLOSE: // 试图关闭窗口 - https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-close
            return self->try_process_close();
        }
    }
    CATCH_FAIL_FAST_MSG("%hs", "unhandled exception in GUI message loop");
    return false;
}

// 设置窗口/子窗口的样式
void window::set_style(HWND item, int style, set_style_mode how)
{
    // 获取样式
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowlongptrw
    auto current_styles = GetWindowLongPtrW(item, GWL_STYLE);
    // 添加或者移除样式相关的 bit，并设置新样式
    auto bits = static_cast<decltype(current_styles)>(style);
    current_styles = (how == set_style_mode::clear)
        ? current_styles bitand (compl bits)
        : current_styles bitor bits;
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowlongptrw
    SetWindowLongPtrW(item, GWL_STYLE, current_styles);
}

// 检测窗口的 Win32 类型名称
bool window::test_class_name(HWND target, std::wstring_view name)
{
    std::wstring buffer;
    // 虽然我们不知道实际上的 Win32 类型名称究竟有多长，
    // 但是，由于我们只是要知道它是否与另一个字符串相等而已，
    // 那么哪怕真的没有拿到完整的字符串，只要拿到的部分比另一个字符串稍长一点，
    // 也足够进行比较了
    // 这里的两个 + 1，一个是为了增加长度，另一个则是为空终止字符预留的位置
    buffer.resize(name.size() + 1 + 1);
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getclassnamew
    auto length = GetClassNameW(target, buffer.data(), buffer.size());
    auto actual_name = buffer.substr(0, length);
    return actual_name == name;
}

bool window::is_button_click_message(WPARAM arg_1, LPARAM arg_2)
{
    if (arg_2 == 0)
    {
        // 这表明发送消息的对象并不是控件
        // https://docs.microsoft.com/en-us/windows/win32/menurc/wm-command#remarks
        return false;
    }
    // 检查是不是按钮点击的消息
    // https://docs.microsoft.com/en-us/windows/win32/controls/bn-clicked
    if (auto notification_code = HIWORD(arg_1);
        notification_code != BN_CLICKED)
    {
        return false;
    }
    // 检查这个控件到底是不是按钮
    // 按理说这个应该放在前面的，但前面那些都只需要比较数字，而这个是字符串，比较慢
    // 所以放在最后检查
    return test_class_name(reinterpret_cast<HWND>(arg_2), WC_BUTTONW);
}

bool window::is_link_notification(LPARAM notification_data)
{
    auto data = reinterpret_cast<NMHDR const*>(notification_data);
    // https://docs.microsoft.com/en-us/windows/win32/controls/nm-click-syslink
    // https://docs.microsoft.com/en-us/windows/win32/api/commctrl/ns-commctrl-nmlink
    return (data->code == NM_CLICK or data->code == NM_RETURN)
        and test_class_name(data->hwndFrom, WC_LINK);
}

bool window::on_initialization(HWND target)
{
    // 保存新创建的窗口的句柄
    m_window = target;
    // 设置窗口以及控件的字体
    // 获取系统的字体
    NONCLIENTMETRICSW system_metrics
    {
        .cbSize = sizeof(NONCLIENTMETRICSW),
    };
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-systemparametersinfow
    THROW_IF_WIN32_BOOL_FALSE(SystemParametersInfoW
    (
        SPI_GETNONCLIENTMETRICS,
        sizeof(system_metrics),
        &system_metrics,
        0
    ));
    // 创建字体
    {
        auto font = CreateFontIndirectW(&system_metrics.lfMessageFont);
        THROW_LAST_ERROR_IF_NULL_MSG(font, "failed to create system font");
        // 保存到智能指针
        m_font.reset(font);
    }
    // 找到所有的控件
    std::vector<HWND> child_controls;
    auto child = GetWindow(m_window, GW_CHILD);
    while (child != nullptr)
    {
        child_controls.push_back(child);
        child = GetWindow(child, GW_HWNDNEXT);
    }
    // 设置字体
    auto font_raw_handle = reinterpret_cast<WPARAM>(m_font.get());
    for (auto control : child_controls)
    {
        // 设置控件的字体
        // https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-setfont
        SendMessageW(control, WM_SETFONT, font_raw_handle, 0);
    }
    SendMessageW(m_window, WM_SETFONT, font_raw_handle, MAKELPARAM(true, 0));
    // 执行那些通过 execute_on_initialization 添加到队列里的函数
    try_execute_queued_functions();
    // https://docs.microsoft.com/en-us/windows/win32/dlgbox/wm-initdialog#return-value
    return true;
}

bool window::try_execute_queued_functions()
{
    // 队列有可能被其他线程改动，需要上锁
    std::unique_lock lock{ m_mutex };
    // 执行队列里的函数
    while (not m_queued_actions.empty())
    {
        auto action = std::move(m_queued_actions.front());
        m_queued_actions.pop();
        lock.unlock(); // 解除锁定，因为被执行的函数可能又会调用添加到队列的函数
        action(m_window);
        lock.lock(); // 执行完毕之后重新锁定
    }
    return true;
}

bool window::try_process_button_click(WPARAM arg_1)
{
    // https://docs.microsoft.com/en-us/windows/win32/controls/bn-clicked
    auto button_id = LOWORD(arg_1);
    if (m_on_button_click != nullptr)
    {
        // 假如用户提供的函数处理了事件，就直接返回
        if (m_on_button_click(m_window, button_id))
        {
            return true;
        }
    }
    // 默认的处理
    switch (button_id)
    {
        // 假如是按了下面这些按钮，设置返回值，然后发送消息尝试关闭窗口
        // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postmessagew
        // https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-close
        case IDOK:
        case IDYES:
            m_result = true;
            PostMessageW(m_window, WM_CLOSE, 0, 0);
            return true;
        case IDCANCEL:
        case IDNO:
        case IDCLOSE:
            m_result = false;
            PostMessageW(m_window, WM_CLOSE, 0, 0);
            return true;
    }
    return false;
}

bool window::try_process_link_notification(LPARAM link_data)
{
    // 当超链接被点击的时候，打开超链接
    // https://docs.microsoft.com/en-us/windows/win32/controls/nm-click-syslink
    auto data = reinterpret_cast<NMLINK const*>(link_data);
    shell::open(data->item.szUrl);
    return true;
}

bool window::try_process_close()
{
    // 关闭对话框，并返回之前设置的返回值
    // 假如没有设置过返回值，默认为 false
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enddialog
    EndDialog(m_window, m_result);
    return true;
}

void progress_data::set_close_window_handler(std::function<bool(HWND)> allow_close_window)
{
    m_allow_close_window = allow_close_window;
}

void progress_data::set_progress(int percentage)
{
    // 获取进度条子窗口
    auto progress_bar = GetDlgItem(m_window, IDC_DOWNLOAD_PROGRESS);
    // 假如之前是滚动的进度条，那么不许滚动
    stop_indeterminate_progress();
    // 设置 0~100 的范围
    // https://docs.microsoft.com/en-us/windows/win32/controls/pbm-setrange
    SendMessageW(progress_bar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    // 设置进度
    // https://docs.microsoft.com/en-us/windows/win32/controls/pbm-setpos
    SendMessageW(progress_bar, PBM_SETPOS, percentage, 0);
}

void progress_data::start_indeterminate_progress()
{
    // 获取进度条子窗口
    auto progress_bar = GetDlgItem(m_window, IDC_DOWNLOAD_PROGRESS);
    // 添加滚动样式
    set_style(progress_bar, PBS_MARQUEE, set_style_mode::set);
    // 开始滚动
    // https://docs.microsoft.com/en-us/windows/win32/controls/pbm-setmarquee
    SendMessageW(progress_bar, PBM_SETMARQUEE, true, 0);
}

void progress_data::stop_indeterminate_progress()
{
    // 获取进度条子窗口
    auto progress_bar = GetDlgItem(m_window, IDC_DOWNLOAD_PROGRESS);
    // 假如本来就不是滚动的进度条，那么什么都不用做
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowlongptrw
    // https://docs.microsoft.com/en-us/windows/win32/controls/progress-bar-control-styles
    if (auto current_styles = GetWindowLongPtrW(progress_bar, GWL_STYLE);
        not (current_styles bitand PBS_MARQUEE))
    {
        return;
    }
    // 停止滚动
    // https://docs.microsoft.com/en-us/windows/win32/controls/pbm-setmarquee
    SendMessageW(progress_bar, PBM_SETMARQUEE, false, 0);
    // 去掉滚动样式
    // https://docs.microsoft.com/en-us/windows/win32/controls/progress-bar-control-styles
    set_style(progress_bar, PBS_MARQUEE, set_style_mode::clear);
}

void progress_data::set_status_text(std::wstring const& text)
{
    SetDlgItemTextW(m_window, IDC_DOWNLOAD_STATUS_TEXT, text.c_str());
}

void progress_data::stop()
{
    // 发送消息尝试关闭窗口
    // https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postmessagew
    // https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-close
    PostMessageW(m_window, WM_CLOSE, 0, 0);
}

// 在窗口关闭之后清空队列、清理资源
void progress_data::clear_queued_actions()
{
    std::scoped_lock lock{ m_mutex };
    m_allow_close_window = nullptr;
    m_on_button_click = nullptr;
    m_queued_actions = {};
    m_window = nullptr;
}

bool progress_data::window_exist() const
{
    std::scoped_lock lock{ m_mutex };
    return m_window != nullptr;
}

bool progress_data::try_process_close()
{
    if (m_allow_close_window != nullptr)
    {
        // 假如不许关闭，那就直接返回、不关闭窗口
        if (not m_allow_close_window(m_window))
        {
            return true;
        }
    }
    return window::try_process_close();
}