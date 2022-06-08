module;
#include <wil/result_macros.h>
#include <concepts>
#include <cwchar>
#include <format>
#include <ranges>
#include <regex>
#include <string>
export module text;

namespace // 供模块内部使用的代码
{
    // 检查参数是否是窄字符串
    template<typename T>
    concept is_narrow_string =
        std::same_as<std::decay_t<T>, std::string>
        or std::same_as<std::decay_t<T>, std::string_view>
        or std::same_as<std::decay_t<T>, char const*>;
}

export namespace text
{
    // 类似于 std::format，但是能把窄字符串（8-bit 字符串，例如 GB2312 或者 UTF-8）
    // 自动转换为 UTF-16
    template<typename... Args>
    std::wstring format(std::wstring_view fmt, Args&&... args);
    // 把窄字符串（8-bit 字符串，例如 GB2312 或者 UTF-8）转换为 UTF-16
    // 使用当前环境的默认编码作为窄字符串的编码
    std::wstring narrow_to_utf16(std::string_view source);
    // 把字符串里的所有 \n 换行符都替换成适合 Windows 界面的 \r\n
    std::wstring to_crlf(std::wstring const& source);
}

namespace // 供模块内部使用的代码
{
    // 假如参数是窄字符串，就尝试把窄字符串自动转换为 UTF-16
    // 否则原样返回
    template<typename T>
    decltype(auto) try_convert_encoding(std::remove_reference_t<T>& t)
    {
        if constexpr(is_narrow_string<T>)
        {
            return text::narrow_to_utf16(t);
        }
        else
        {
            return std::forward<T>(t);
        }
    }
    template<typename T>
    decltype(auto) try_convert_encoding(std::remove_reference_t<T>&& t)
    {
        if constexpr(is_narrow_string<T>)
        {
            return text::narrow_to_utf16(t);
        }
        else
        {
            return std::forward<T>(t);
        }
    }
}

// 类似于 std::format，但是能把窄字符串（8-bit 字符串，例如 GB2312 或者 UTF-8）
// 自动转换为 UTF-16
template<typename... Args>
std::wstring text::format(std::wstring_view fmt, Args&&... args)
{
    return std::vformat
    (
        fmt,
        std::make_wformat_args(try_convert_encoding<Args>(args)...)
    );
}

module:private;

// 把窄字符串（8 字节字符串，例如 GB2312 或者 UTF-8）转换为 UTF-16
// 使用当前环境的默认编码作为窄字符串的编码
std::wstring text::narrow_to_utf16(std::string_view source)
{
    std::wstring utf16_string;
    // 使用 std::mbrtowc 进行编码转换：
    // https://en.cppreference.com/w/cpp/string/multibyte/mbrtowc
    // 本来是想用更加完善的 std::mbrtoc16 的，可惜 MSVC 太坑了！
    // 微软的 mbrtoc16 只支持 UTF-8，不支持当前环境的默认编码……
    std::mbstate_t state{};
    while (not source.empty())
    {
        // char16_t utf16 = 0; // std::mbrtoc16 使用的类型
        wchar_t utf16 = 0; // std::mbrtowc 使用的类型
        auto current = source.data();
        auto size = source.size();
        // auto result = std::mbrtoc16(&utf16, current, size, &state);
        auto result = std::mbrtowc(&utf16, current, size, &state);
        // -1, -2 代表错误
        if ((result == static_cast<std::size_t>(-1))
            or (result == static_cast<std::size_t>(-2)))
        {
            utf16_string += L"<narrow_to_utf16() failed>";
            return utf16_string;
        }
        // -3 代表输出的字符属于代理对（Surrogate Pair）
        // 在那种情况下，它并没有处理更多的窄字符
        // 不过，只有 std::mbrtoc16 才会返回 -3
        // std::mbrtowc 没有 Surrogate Pair 的概念，永远不会返回 -3
        else if (result != static_cast<std::size_t>(-3))
        {
            // 假如不是 -3，就表明读取并处理了 result 个窄字符
            // 把已经处理的窄字符去掉
            // 不过，假如返回值是 0，并不表明没有处理任何窄字符
            // 而是代表处理了一个空字符，空字符的长度也是 1
            auto length = std::max<std::size_t>(1, result);
            FAIL_FAST_IF(length > source.length());
            source.remove_prefix(length);
        }
        utf16_string += static_cast<wchar_t>(utf16);
    }
    return utf16_string;
}

// 把字符串里的所有 \n 换行符都替换成适合 Windows 界面的 \r\n
std::wstring text::to_crlf(std::wstring const& source)
{
    // 尝试把所有的 \n 都替换成 \r\n
    // 假如本来就是 \r\n，则应该保持不变
    std::wregex new_line_regex{ L"\\r?\\n" };
    return std::regex_replace(source, new_line_regex, L"\r\n");
}