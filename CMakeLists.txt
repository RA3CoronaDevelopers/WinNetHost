cmake_minimum_required(VERSION 3.17)
# 确保 VCPKG 编译的依赖也会被静态链接
set(VCPKG_TARGET_TRIPLET "x86-windows-static")
# 声明我们的项目
project(NativeHost)

# 第三方库
# NET Host，在 C++ 程序里启动 .NET 运行时
find_package(unofficial-nethost CONFIG REQUIRED)
# WIL,一个方便调用 Win32 API 的库
find_package(wil CONFIG REQUIRED)

# 创建一个编译到 exe 的目标
add_executable(${PROJECT_NAME} WIN32)
# 确保我们在使用 MultiThreaded / MultiThreadedDebug
# 而不是 MultiThreadedDll / MultiThreadedDebugDll
# 以避免需要依赖
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# 为我们的目标启用 C++20
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
# 定义宏 - 定义 WIN32_LEAN_AND_MEAN 可以去除掉 Windows.h 里各种不必要的内容
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32_LEAN_AND_MEAN
)
# 链接依赖
target_link_libraries(${PROJECT_NAME} PRIVATE Comctl32.lib)
target_link_libraries(${PROJECT_NAME} PRIVATE
    unofficial::nethost::nethost
    WIL::WIL
)
# 源码
target_sources(${PROJECT_NAME} PRIVATE
    main.cpp gui.ixx hostfxr.ixx error_handling.ixx
    process.ixx safe_load_dll_feature.ixx shell.ixx text.ixx
)
# 资源文件
target_sources(${PROJECT_NAME} PRIVATE resource.h gui.rc)
# 应用清单
target_sources(${PROJECT_NAME} PRIVATE app.manifest)

# （暂时被禁用）在执行 CMake install 步骤的时候，把产生的 exe 复制到定义的地方
# install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ".")
