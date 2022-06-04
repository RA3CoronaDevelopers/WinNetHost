// ϵͳ��
#include <Windows.h>
#include <ShellApi.h>
// vcpkg
#include <wil/resource.h>
// ��׼��
#include <cwchar>
#include <filesystem>
#include <format>
#include <span>
#include <string_view>
// �޲�����
#include "patch-runtime.h"
// ģ��
import hostfxr;
import error_handling;

namespace
{
    // ��խ�ַ�����8 �ֽ��ַ��������� GB18030 ���� UTF-8��ת��Ϊ UTF-16
    // ʹ�õ�ǰ������Ĭ�ϱ�����Ϊխ�ַ����ı���
    std::wstring narrow_to_utf16(char const* source);
}

// Windows �� Main ����
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    // ���� error_handling.ixx ��� initialize_error_handling
    // ��ʼ��������Ȼ���� execute_protected ִ�д���
    // �������������ʱ�������ܿ�����������
    initialize_error_handling();
    return execute_protected([]
    {
        // ʹ�� CommandLineToArgvW �ָ������в���
        // CommandLineToArgvW ���ص��ڴ���Ҫ���� LocalFree �ͷţ�
        // https://docs.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-commandlinetoargvw#remarks
        // �� wil::unique_hlocal_ptr ���þ���һ��ʹ�� LocalFree �������ڴ�����ͣ�
        // https://github.com/Microsoft/wil/wiki/RAII-resource-wrappers#available-unique_any-simple-memory-patterns
        // ������ǿ���ֱ��ʹ����
        auto argc = 0;
        wil::unique_hlocal_ptr<wchar_t*> argv;
        argv.reset(CommandLineToArgvW(GetCommandLineW(), &argc));
        // ͨ�� hostfxr.ixx ��� hostfxr_dll ���� DLL��
        // ���سɹ�֮������ hostfxr_app_context ���� .NET Ӧ��
        try
        {
            hostfxr_dll::load();
        }
        catch (std::exception const& e)
        {
            // �� std::exception::what() �� char* ת���� UTF-16 �Ŀ��ַ���
            auto reason = narrow_to_utf16(e.what());
            // ������Ϣ
            auto message = std::format(L"HostFxr.dll ����ʧ�ܣ���������Ϊ����ͻ������˰�װ .NET ���п�\n{}", reason);
            // ���� error_handling.ixx ��ĵ���������ʾһ����ª�ĵ���
            show_error_message_box(message.c_str());
            return 1;
        }
        try
        {
            // ����� std::span �� C# �� Span �����һ����
            // ��֮�������� span ���ݲ��������� .NET Ӧ��
            std::span arguments{ argv.get(), static_cast<std::size_t>(argc) };
            // TODO: �����д CoronaLauncher.dll ������ڡ���ǰ·������
            // ���ǵ�ǰ·����ʵ��һ�����Ǳ� EXE ������·��
            // ����Ҫ���ӿɿ��Ļ�������������
            // #include <wil/stl.h>
            // #include <wil/win32_helpers.h>
            // https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-queryfullprocessimagenamew
            // https://github.com/microsoft/wil/wiki/Win32-helpers#predefined-adapters-for-functions-that-return-variable-length-strings
            // auto this_exe = wil::QueryFullProcessImageNameW<std::wstring>();
            // std::filesystem::path result{ this_exe };
            // result.replace_filename("bin/CoronaLauncher.dll")
            // Ȼ��� result ��Ϊ�������� hostfxr_app_context::load() ����
            hostfxr_app_context::load(arguments, "CoronaLauncher.dll");
        }
        catch (std::exception const& e)
        {
            // �� std::exception::what() �� char* ת���� UTF-16 �Ŀ��ַ���
            auto reason = narrow_to_utf16(e.what());
            // ������Ϣ
            auto message = std::format(L"CoronaLauncher.dll ����ʧ�ܣ���������Ϊ����ͻ��˵İ�װ������\n{}", reason);
            // ���� error_handling.ixx ��ĵ���������ʾһ����ª�ĵ���
            show_error_message_box(message.c_str());
            return 1;
        }
        // ���� .NET Ӧ��
        // ��ʵ��hostfxr_app_context::load() Ҳ�᷵�����������
        // Ҳ������ʵ����������
        // hostfxr_app_context::load(...).run();
        // �������ֿ����Ļ�������д����ϸ�ֵ� try / catch
        // Ҳ�������״�����ܳ��ֵı���
        return hostfxr_app_context::get().run();
    });
}

namespace
{
    // ��խ�ַ�����8 �ֽ��ַ��������� GB18030 ���� UTF-8��ת��Ϊ UTF-16
    // ʹ�õ�ǰ������Ĭ�ϱ�����Ϊխ�ַ����ı���
    std::wstring narrow_to_utf16(char const* source)
    {
        std::wstring result;
        // ʹ�� std::mbsrtowcs ���б���ת����
        // https://en.cppreference.com/w/cpp/string/multibyte/mbsrtowcs
        std::mbstate_t state{};
        // ��һ�ε��ã��Ȳ�ִ��������ת�������Ǵ��� null��
        // �� std::mbsrtowcs ������Ҫ���ַ�������
        auto length = std::mbsrtowcs(nullptr, &source, 0, &state);
        if (length == static_cast<std::size_t>(-1))
        {
            return L"<narrow_to_utf16() failed>";
        }
        result.resize(length);
        // ����������ת��
        std::mbsrtowcs(result.data(), &source, result.size(), &state);
        return result;
    }
}