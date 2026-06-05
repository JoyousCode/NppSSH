#!/bin/bash
set -euo pipefail  # 开启严格模式：报错立即退出、未定义变量报错、管道错误传递

# 配置项（大写+下划线规范）
MYSQL_PASSWORD="123456"
CONTAINER_NAME="mysql"
ERROR_LOG="/tmp/mysql_error.log"
SCRIPT_NAME=$(basename $0)

# 清理日志函数（原子操作）
clear_log() {
    [ -f "$ERROR_LOG" ] && rm -f "$ERROR_LOG"
}

# 前置检查函数
pre_check() {
    echo -e "\n================================================="
    echo "  Docker MySQL 自动化测试脚本（$SCRIPT_NAME）"
    echo "  异常自动回退，不残留数据"
    echo -e "=================================================\n"

    # 检查docker是否可用
    if ! command -v docker &>/dev/null; then
        echo "【ERROR】Docker 未安装或未启动！"
        exit 1
    fi

    # 检查容器是否运行（优化判断逻辑）
    if ! docker inspect "$CONTAINER_NAME" &>/dev/null; then
        echo "【ERROR】容器 $CONTAINER_NAME 不存在！"
        exit 1
    fi
    CONTAINER_RUNNING=$(docker inspect --format '{{.State.Running}}' "$CONTAINER_NAME")
    if [ "$CONTAINER_RUNNING" != "true" ]; then
        echo "【ERROR】容器 $CONTAINER_NAME 未运行！当前状态：$CONTAINER_RUNNING"
        exit 1
    fi

    # 检查MySQL连接是否正常（屏蔽无关输出）
    docker exec "$CONTAINER_NAME" mysql -u root -p"$MYSQL_PASSWORD" -e "SELECT 1" 2>"$ERROR_LOG"
    if [ $? -ne 0 ]; then
        echo "【ERROR】MySQL连接失败！错误信息："
        cat "$ERROR_LOG"
        clear_log
        exit 1
    fi

    clear_log
}

# 执行SQL函数（增强容错+标准化输出）
exec_sql() {
    local sql="$1"
    local step_msg="$2"
    local exit_code=0

    echo -e "\n【步骤】$step_msg"
    echo "【执行SQL】$sql"
    echo "----------------------------------------"
    
    # 执行SQL：屏蔽密码警告，仅保留真实错误
    docker exec -i "$CONTAINER_NAME" mysql -u root -p"$MYSQL_PASSWORD" -N -B 2>"$ERROR_LOG" << EOF
$sql
EOF
    exit_code=$?

    # 错误处理
    if [ $exit_code -ne 0 ]; then
        echo -e "【ERROR】执行失败！错误信息："
        grep -v -E "Warning|Using a password on the command line interface can be insecure" "$ERROR_LOG" || true
        echo -e "\n【回退】出现异常，开始回退所有操作并退出！"
        clean_up
        exit $exit_code
    fi

    echo "【OK】执行成功"
    echo "================================================="
}

# 清理回退函数（幂等+静默执行）
clean_up() {
    echo -e "\n【清理】开始清理环境（确保无残留数据）..."

    # 批量清理：静默执行，避免清理失败导致脚本报错
    docker exec -i "$CONTAINER_NAME" mysql -u root -p"$MYSQL_PASSWORD" -s -N 2>/dev/null << EOF
DROP TABLE IF EXISTS school.student;
DROP DATABASE IF EXISTS school;
EOF

    # 清理日志
    clear_log
    
    echo "【清理】环境清理完成！"
}

# ===================== 主流程 =====================
# 捕获脚本中断信号，确保清理执行
trap 'echo -e "\n【中断】脚本被手动中断，执行清理..."; clean_up; exit 1' SIGINT SIGTERM

# 前置检查
pre_check

# 1、查看所有数据库
exec_sql "SHOW DATABASES;" "查看当前所有数据库"

# 2、创建school数据库
exec_sql "CREATE DATABASE IF NOT EXISTS school;" "创建 school 数据库"

# 3、创建 student 表（显式指定school数据库）
exec_sql "
CREATE TABLE IF NOT EXISTS school.student (
    name VARCHAR(50),
    age INT,
    current TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);" "创建 student 表（name,age,current）"

# 4、插入数据 zhaoxuandong 24
exec_sql "INSERT INTO school.student (name,age) VALUES ('zhaoxuandong',24);" "插入数据：zhaoxuandong,24"

# 5、查询所有数据
exec_sql "SELECT * FROM school.student;" "查询 student 表所有内容"

# 6、删除所有数据
exec_sql "DELETE FROM school.student;" "删除 student 表所有数据"

# 7、删除表
exec_sql "DROP TABLE IF EXISTS school.student;" "删除 student 表"

# 8、删除数据库
exec_sql "DROP DATABASE IF EXISTS school;" "删除 school 数据库"

# ===================== 执行完成 =====================
echo -e "\n================================================="
echo " 【SUCCESS】所有步骤全部执行完成！"
echo " 无残留数据，无异常"
echo -e "=================================================\n"

# 最终清理（确保干净）
clean_up

# 脚本正常退出
exit 0