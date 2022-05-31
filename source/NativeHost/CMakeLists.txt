cmake_minimum_required(VERSION 3.17)
# 确保 VCPKG 编译的依赖也会被静态链接
set(VCPKG_TARGET_TRIPLET "x86-windows-static")
# 声明我们的项目
project(NativeHost)

# 找到 NET Host 的依赖
find_package(nethost CONFIG REQUIRED)

set(OPTIONS WIN32)
# 创建一个编译到 exe 的目标
add_executable(${PROJECT_NAME})
# 确保我们在使用 MultiThreaded / MultiThreadedDebug
# 而不是 MultiThreadedDll / MultiThreadedDebugDll
# 以避免需要依赖
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# 为我们的目标启用 C++20
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
# 链接 NET Host 的依赖
target_link_libraries(${PROJECT_NAME} PRIVATE libnethost)
# 源码
target_sources(${PROJECT_NAME} PRIVATE nativehost.cpp)
# 假如定义了输出位置，就把产生的 exe 复制到那里
if (${NATIVE_HOST_EXE})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${NATIVE_HOST_EXE}
    )
endif()
