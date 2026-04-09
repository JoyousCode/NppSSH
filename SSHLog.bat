@echo off
setlocal enabledelayedexpansion

set "logfile=C:\Users\Admin\AppData\Roaming\Notepad++\plugins\config\NppSSH.log"

:menu
cls
echo ================================
echo    日志文件实时监控
echo ================================
echo 1. 从文件开头开始监控
echo 2. 只监控最新内容
echo 3. 监控最新10行
echo 4. 指定监控行数
echo 5. 退出
echo ================================
set /p choice="请选择 (1-5): "

if "!choice!"=="1" (
    goto :start_full
) else if "!choice!"=="2" (
    goto :start_tail
) else if "!choice!"=="3" (
    set "lines=10"
    goto :start_lines
) else if "!choice!"=="4" (
    set /p lines="请输入要显示的行数: "
    goto :start_lines
) else if "!choice!"=="5" (
    exit
) else (
    echo 无效选择，按任意键重试...
    pause >nul
    goto :menu
)

:start_full
cls
echo 正在从文件开头监控 %logfile%...
echo 按 Ctrl+C 停止
echo.
powershell -Command "Get-Content '%logfile%' -Wait"
goto :end

:start_tail
cls
echo 正在监控 %logfile% 的最新内容...
echo 按 Ctrl+C 停止
echo.
powershell -Command "Get-Content '%logfile%' -Wait -Tail 0"
goto :end

:start_lines
cls
echo 正在监控 %logfile% 的最新 !lines! 行...
echo 按 Ctrl+C 停止
echo.
powershell -Command "Get-Content '%logfile%' -Wait -Tail !lines!"
goto :end

:end
echo.
echo 监控已结束
pause