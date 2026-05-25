#!/bin/bash

# 配置项
MYSQL_PWD="123456"
CONTAINER_NAME="mysql"
ERROR_LOG="/tmp/mysql_error.log"

# 清理日志函数
clear_log() {
    rm -f $ERROR_LOG
}

# 前置检查函数
pre_check() {
    echo "================================================"
    echo "  Docker MySQL 自动化测试脚本（一步一打印）"
    echo "  异常自动回退，不残留数据"
    echo "================================================"
    echo ""

    # 检查容器是否运行
    if ! docker inspect --format '{{.State.Running}}' $CONTAINER_NAME 2>/dev/null | grep -q "true"; then
        echo "【ERROR】容器 $CONTAINER_NAME 未运行！"
        exit 1
    fi

    # 检查MySQL连接是否正常
    docker exec $CONTAINER_NAME mysql -u root -p$MYSQL_PWD -e "SELECT 1" 2>$ERROR_LOG
    if [ $? -ne 0 ]; then
        echo "【ERROR】MySQL连接失败！错误信息："
        cat $ERROR_LOG
        exit 1
    fi

    clear_log
}

# 执行SQL函数（优化版）
exec_sql() {
    local sql="$1"
    local step_msg="$2"
    
    echo "【步骤】$step_msg"
    echo "【执行SQL】$sql"
    echo "----------------------------------------"
    
    # 执行SQL，过滤密码安全警告，仅保留真实错误
    docker exec -i $CONTAINER_NAME mysql -u root -p$MYSQL_PWD -N -B 2>$ERROR_LOG << EOF
$sql
EOF

    # 判断执行结果
    local exit_code=$?
    if [ $exit_code -ne 0 ]; then
        echo "【ERROR】执行失败！错误信息："
        # 过滤掉密码安全警告，只显示真实错误
        grep -v "Warning" $ERROR_LOG
        echo ""
        echo "【回退】出现异常，开始回退所有操作并退出！"
        clean_up
        exit $exit_code
    fi

    echo "【OK】执行成功"
    echo "================================================="
    echo ""
}

# 清理回退函数（异常/结束都会调用）
clean_up() {
    echo "【回退/清理】开始清理环境..."

    # 批量清理，显式指定数据库，避免依赖USE
    docker exec -i $CONTAINER_NAME mysql -u root -p$MYSQL_PWD 2>/dev/null << EOF
DROP TABLE IF EXISTS school.student;
DROP DATABASE IF EXISTS school;
EOF

    # 清理日志
    clear_log
    
    echo "【回退/清理】清理完成，无残留数据"
    echo ""
}

# ===================== 主流程 =====================
# 前置检查
pre_check

# 1、查看所有数据库
exec_sql "SHOW DATABASES;" "查看当前所有数据库"

# 2、创建school数据库
exec_sql "CREATE DATABASE IF NOT EXISTS school;" "创建 school 数据库"

# 3、创建 student 表（显式指定school数据库，无需USE）
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
echo ""
echo "================================================"
echo " 【SUCCESS】所有步骤全部执行完成！"
echo " 无残留数据，无异常"
echo "================================================"
echo ""

# 最终清理（确保干净）
clean_up

exit 0