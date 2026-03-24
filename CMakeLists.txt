# CMakeList.txt: CMackBase 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required(VERSION 4.2.3-msvc3)
project(NppSSH LANGUAGES C CXX)

if(MSVC)
    # 强制 UTF-8 + Unicode + 64位 + 动态CRT
    add_compile_options("/source-charset:utf-8" "/execution-charset:utf-8")
    add_compile_definitions(
        UNICODE 
        _UNICODE 
        WIN64 
        NPP_PLUGIN 
        UNICODE_PLUGIN=1
    )
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob2 /W3")
    set(CMAKE_C_FLAGS_RELEASE "/MD /O2 /Ob2 /W3")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(LIBSSH2_INC "C:/Users/Admin/Desktop/project/C++(notepad++)/notepadSdk/libssh2/install_x64/include")
set(LIBSSH2_LIB "C:/Users/Admin/Desktop/project/C++(notepad++)/notepadSdk/libssh2/install_x64/lib/libssh2.lib")
set(NOTEPADSDK_DIR "C:/Users/Admin/Desktop/project/C++(notepad++)/notepadSdk/plugintemplate/src")

add_library(NppSSH SHARED
    src/PluginDefinition.cpp
    src/NppPluginDemo.cpp
    src/PluginDefinition.h
    src/PluginInterface.h
    src/Scintilla.h
    src/Sci_Position.h
    src/menuCmdID.h
    src/Notepad_plus_msgs.h
)


target_include_directories(NppSSH PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${NOTEPADSDK_DIR}
    ${LIBSSH2_INC}
)
target_link_libraries(NppSSH PRIVATE
    ${LIBSSH2_LIB}
    ws2_32.lib
)



set_target_properties(NppSSH PROPERTIES
    OUTPUT_NAME "NppSSH"
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY "D:/software/application/Notepad++/plugins/NppSSH"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "D:/software/application/Notepad++/plugins/NppSSH"
    WINDOWS_EXPORT_ALL_SYMBOLS OFF  # 禁用自动导出，仅手动导出
    ARCHITECTURE "x64"
)
