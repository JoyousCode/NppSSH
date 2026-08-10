# CMakeList.txt: CMackBase 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required(VERSION 3.25)
project(NppSSH LANGUAGES C CXX)

# ========== 配置阶段强制生成带正确exe的launch.vs.json 
set(NPP_HOST_EXE "D:\\\\software\\\\developer\\\\Notepad++\\\\Notepad++.exe")
set(VS_LAUNCH_FILE "${CMAKE_SOURCE_DIR}/.vs/launch.vs.json")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/.vs")
# 直接覆盖写入标准dll类型配置
file(WRITE "${VS_LAUNCH_FILE}"
"{
  \"version\": \"0.2.1\",
  \"defaults\": {},
  \"configurations\": [
    {
      \"type\": \"dll\",
      \"exe\": \"${NPP_HOST_EXE}\",
      \"project\": \"CMakeLists.txt\",
      \"projectTarget\": \"NppSSH.dll (D:\\\\software\\\\developer\\\\Notepad++\\\\plugins\\\\NppSSH\\\\NppSSH.dll)\",
      \"name\": \"NppSSH.dll (D:\\\\software\\\\developer\\\\Notepad++\\\\plugins\\\\NppSSH\\\\NppSSH.dll)\"
    }
  ]
}"
)
# 设置文件只读，阻止VS清空exe字段
if(WIN32)
    execute_process(COMMAND attrib +r "${VS_LAUNCH_FILE}")
endif()

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
set(LIBSSH2_INC "${CMAKE_SOURCE_DIR}/src/res/sshlib2_install_x64/include")
set(LIBSSH2_LIB "${CMAKE_SOURCE_DIR}/src/res/sshlib2_install_x64/lib/libssh2.lib")

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
    src/Windows/SSHTermPanel.h
    src/Windows/SSHTermPanel.cpp
    src/Windows/SSHConnection.h
    src/Windows/SSHConnection.cpp
    src/SSHSettings.h
    src/SSHSettings.cpp

    src/res/connect.ico
    src/res/disconnect.ico
    src/res/EyeHide.ico
    src/res/EyeShow.ico
    src/res/nppssh.ico
    src/res/putty.ico
    src/res/selectfile.ico
    src/res/close.ico
    src/Windows/SSHLog.h
    src/Windows/SSHLog.cpp
    src/Windows/SSHTerminal.h
    src/Windows/SSHTerminal.cpp
    src/Windows/SSHUtil.h
    src/Windows/SSHUtil.cpp
    src/SSHClassUtil.h
    src/Windows/SSHBasePanel.h
    src/Windows/SSHBasePanel.cpp
    src/Windows/SSHAppPanel.h
    src/Windows/SSHAppPanel.cpp
)
# 新增：指定 RC 文件为资源编译（Windows 特有）
if(WIN32)
    set_source_files_properties(
        src/SSHClient.rc
        PROPERTIES
        OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/src/res/connect.ico;${CMAKE_SOURCE_DIR}/src/res/disconnect.ico;${CMAKE_SOURCE_DIR}/src/res/EyeHide.ico;${CMAKE_SOURCE_DIR}/src/res/EyeShow.ico;${CMAKE_SOURCE_DIR}/src/res/nppssh.ico"
    )
endif()


target_include_directories(NppSSH PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${LIBSSH2_INC}
)
target_link_libraries(NppSSH PRIVATE
    ${LIBSSH2_LIB}
    ws2_32.lib
    shlwapi.lib
    advapi32.lib
    crypt32.lib
    imm32.lib
    gdiplus.lib
    user32.lib
    gdi32.lib
)
target_compile_definitions(NppSSH PRIVATE
    WIN32_LEAN_AND_MEAN
    _WINSOCKAPI_
    UNICODE
    _UNICODE
    NOMINMAX
)

set_target_properties(NppSSH PROPERTIES
    OUTPUT_NAME "NppSSH"
    SUFFIX ".dll"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "D:/software/developer/Notepad++/plugins/NppSSH"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "D:/software/developer/Notepad++/plugins/NppSSH"
    WINDOWS_EXPORT_ALL_SYMBOLS OFF  # 禁用自动导出，仅手动导出
    ARCHITECTURE "x64"
    LINK_FLAGS "/DLL"
)
