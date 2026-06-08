@echo off
setlocal enabledelayedexpansion
chcp 936 >nul 2>&1

:MainLoop
cls
echo "=============================================="
echo "        Notepad++安全关闭管理工具"
echo "=============================================="
echo "【1】检测Notepad++运行状态，列出进程PID"
echo "【2】模拟点击窗口右上角X，安全正常关闭程序"
echo "【3】强制终止Notepad++进程（备选兜底）"
echo "=============================================="
echo "注：空回车直接执行【2】"
set "choice="
set /p choice=请输入数字(1/2/3)执行操作：

:: 空回车直接刷新菜单
if "!choice!"=="" (
    echo.
    echo "正在检索Notepad++(notepad++.exe)进程信息..."
    set "isRun=0"
    :: 使用findstr直接查找进程，避免CSV解析问题
    for /f "tokens=2" %%a in ('tasklist /nh /fi "imagename eq notepad++.exe" 2^>nul ^| findstr /i "notepad++"') do (
        set "isRun=1"
        echo "发现运行进程，PID=%%a"
		echo.
		echo "尝试模拟点击Notepad++窗口右上角关闭按钮，安全退出..."
		powershell -Command "$proc=Get-Process notepad++ -ErrorAction SilentlyContinue;if($proc){$proc.CloseMainWindow()}"
		echo "发送关闭窗口消息完成（等待软件安全退出）"
		:WaitExit
		timeout /t 1 /nobreak >nul
		tasklist /nh /fi "imagename eq notepad++.exe" 2>nul | findstr /i "notepad++" >nul
		if !errorlevel! equ 0 (
			echo "仍在等待Notepad++退出..."
			goto WaitExit
		)
		echo "Notepad++已完全退出，执行复检..."
    )
    if !isRun! equ 0 (
        echo "当前无Notepad++程序正在运行"
    )
	
	echo.
	echo "操作完成，自动重新执行一次【1】检测..."
	call :RunCheck
	pause >nul
	goto MainLoop
)

::=== 选项1：检测进程 ===
if "!choice!"=="1" (
    echo.
    echo "正在检索Notepad++(notepad++.exe)进程信息..."
    set "isRun=0"
    :: 使用findstr直接查找进程，避免CSV解析问题
    for /f "tokens=2" %%a in ('tasklist /nh /fi "imagename eq notepad++.exe" 2^>nul ^| findstr /i "notepad++"') do (
        set "isRun=1"
        echo "发现运行进程，PID=%%a"
    )
    if !isRun! equ 0 (
        echo "当前无Notepad++程序正在运行"
    )
    echo.
    echo "操作完成，自动重新执行一次【1】检测..."
    call :RunCheck
    pause >nul
    goto MainLoop
)

::=== 选项2：模拟点击右上角X ===
if "!choice!"=="2" (
    echo.
    echo "尝试模拟点击Notepad++窗口右上角关闭按钮，安全退出..."
    powershell -Command "$proc=Get-Process notepad++ -ErrorAction SilentlyContinue;if($proc){$proc.CloseMainWindow()}"
    echo "发送关闭窗口消息完成（等待软件自主保存退出）"
	
	:WaitExit
    timeout /t 1 /nobreak >nul
    tasklist /nh /fi "imagename eq notepad++.exe" 2>nul | findstr /i "notepad++" >nul
    if !errorlevel! equ 0 (
        echo "仍在等待Notepad++退出..."
        goto WaitExit
    )
    echo "Notepad++已完全退出，执行复检..."
	
    echo.
    echo "操作完成，自动重新执行一次【1】检测..."
    call :RunCheck
    pause >nul
    goto MainLoop
)

::=== 选项3：强制结束进程 ===
if "!choice!"=="3" (
    echo.
    echo "正在执行强制结束进程操作..."
    taskkill /im notepad++.exe /f >nul 2>&1
    echo "强制终止指令已执行完毕"
    echo.
    echo "操作完成，自动重新执行一次【1】检测..."
    call :RunCheck
    pause >nul
    goto MainLoop
)

::==== 无效输入处理 ====
echo.
echo "输入无效！仅可输入 1、2、3"
echo.
:: 错误输入无需按回车，直接返回
goto MainLoop

::==== 自动复检子程序 ====
:RunCheck
echo.
echo "-------- 自动复检选项【1】 --------"
set "checkRun=0"
for /f "tokens=2" %%b in ('tasklist /nh /fi "imagename eq notepad++.exe" 2^>nul ^| findstr /i "notepad++"') do (
    set "checkRun=1"
    echo "复检仍在运行，PID=%%b"
)
if !checkRun! equ 0 (
    echo "复检结果：Notepad++已全部关闭"
)
echo "----------------------------------"
echo.
exit /b