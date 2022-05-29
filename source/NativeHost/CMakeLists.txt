cmake_minimum_required(VERSION 3.17)
project(NativeHost)

set(OPTIONS WIN32)
add_executable(${PROJECT_NAME})
# 确保我们在使用 MultiThreaded / MultiThreadedDebug
# 而不是 MultiThreadedDll / MultiThreadedDebugDll
# 以避免需要依赖
set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_include_directories(${PROJECT_NAME} PRIVATE inc inc.vs)
target_sources(${PROJECT_NAME} PRIVATE nativehost.cpp)
target_sources(${PROJECT_NAME} PRIVATE inc/coreclr_delegates.h inc/hostfxr.h)
target_sources(${PROJECT_NAME} PRIVATE inc.vs/nethost.h)