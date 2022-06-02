cmake_minimum_required(VERSION 3.17)
# 确保 VCPKG 编译的依赖也会被静态链接
set(VCPKG_TARGET_TRIPLET "x86-windows-static")
# 声明我们的项目
project(NativeHost)

# 找到 NET Host 的依赖
find_package(unofficial-nethost CONFIG REQUIRED)
# 找到 WIL（一个方便调用 Win32 API 的库）的依赖
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
target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN)
# 链接依赖
target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::nethost::nethost WIL::WIL)
# 源码
target_sources(${PROJECT_NAME} PRIVATE nativehost.cpp hostfxr.ixx error_handling.ixx)
# 把产生的 exe 复制到定义的地方
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ".")