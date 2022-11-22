cmake_minimum_required(VERSION 3.17)
project(NativeHost)

# 第三方库（参见 vcpkg.json）
# NET Host，在 C++ 程序里启动 .NET 运行时
find_package(unofficial-nethost CONFIG REQUIRED)
# WIL,一个方便调用 Win32 API 的库
find_package(wil CONFIG REQUIRED)

# 创建一个编译到 exe 的目标
add_executable(${PROJECT_NAME} WIN32)
# 源码
add_subdirectory("source/NativeHost")
# 确保我们在使用 MultiThreaded / MultiThreadedDebug
# 而不是 MultiThreadedDll / MultiThreadedDebugDll
# 静态链接 C++ 运行库，而不是要求用户电脑上事先安装 C++ 运行库
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# 为我们的目标启用 C++20
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
# 定义宏
# - 定义 WIN32_LEAN_AND_MEAN 可以去除掉 Windows.h 里各种不必要的内容
# - 启用宽字符 API（默认使用 UTF-16 而不是 GB2312 之类的窄字符编码）
# - 把最低支持的操作系统设为 Win7
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "WIN32_LEAN_AND_MEAN=1"
    "UNICODE=1"
    "_UNICODE=1"
    "WINVER=_WIN32_WINNT_WIN7"
    "_WIN32_WINNT=_WIN32_WINNT_WIN7"
)
# 强制把源码视为 UTF-8（否则 MSVC 会尝试使用 GB2312）
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<C_COMPILER_ID:MSVC>:/utf-8>")
target_compile_options(${PROJECT_NAME} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
# 链接依赖
target_link_libraries(${PROJECT_NAME} PRIVATE Comctl32.lib)
target_link_libraries(${PROJECT_NAME} PRIVATE
    unofficial::nethost::nethost
    WIL::WIL
)

# （暂时被禁用）在执行 CMake install 步骤的时候，把产生的 exe 复制到定义的地方
# install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ".")
