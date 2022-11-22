# 源码
target_sources(${PROJECT_NAME} PRIVATE
    main.cpp gui.ixx hostfxr.ixx error_handling.ixx
    process.ixx safe_load_dll_feature.ixx shell.ixx text.ixx
)
# 资源文件
target_sources(${PROJECT_NAME} PRIVATE resource.h gui.rc)
# 应用清单
target_sources(${PROJECT_NAME} PRIVATE app.manifest)
