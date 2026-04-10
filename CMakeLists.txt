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
    
    src/SSHClient.cpp
    src/PluginDefinition.h
    src/PluginInterface.h
    src/Scintilla.h
    src/Sci_Position.h
    src/menuCmdID.h
    src/Notepad_plus_msgs.h
    src/SSHClient.h
    src/DockingFeature/Docking.h
    src/DockingFeature/DockingDlgInterface.h
    src/DockingFeature/dockingResource.h
    src/DockingFeature/GoToLineDlg.cpp
    src/DockingFeature/GoToLineDlg.h
    src/DockingFeature/resource.h
    src/DockingFeature/StaticDialog.cpp
    src/DockingFeature/StaticDialog.h
    src/DockingFeature/Window.h
    src/Resource.h
    src/NppPluginSSH.cpp
    src/SSHClient.rc

    src/windows/SSHWindow.cpp
    src/Windows/SSHWindow.h
    src/Windows/SSHPanel.h
    src/Windows/SSHPanel.cpp
    src/Windows/SSHConnection.h
    src/Windows/SSHConnection.cpp
    src/SSHSettings.h
    src/SSHSettings.cpp

    src/res/connect.ico
    src/res/disconnect.ico
    src/res/nppssh.ico
    src/Windows/SSHLog.h
    src/Windows/SSHLog.cpp
)
# 新增：指定 RC 文件为资源编译（Windows 特有）
if(WIN32)
    set_source_files_properties(
        src/SSHClient.rc
        PROPERTIES
        OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/src/res/connect.ico;${CMAKE_SOURCE_DIR}/src/res/disconnect.ico"
    )
endif()


target_include_directories(NppSSH PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${NOTEPADSDK_DIR}
    ${LIBSSH2_INC}
)
target_link_libraries(NppSSH PRIVATE
    ${LIBSSH2_LIB}
    ws2_32.lib
    shlwapi.lib
    advapi32.lib
    crypt32.lib
)



set_target_properties(NppSSH PROPERTIES
    OUTPUT_NAME "NppSSH"
    SUFFIX ".dll"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "D:/software/developer/Notepad++/plugins/NppSSH"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "D:/software/developer/Notepad++/plugins/NppSSH"
    WINDOWS_EXPORT_ALL_SYMBOLS OFF  # 禁用自动导出，仅手动导出
    ARCHITECTURE "x64"
)
