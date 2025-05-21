#!/bin/bash

set -e

# 获取当前路径
PROJECT_ROOT=$(pwd)

# 创建 build 目录（如果不存在）
if [ ! -d "$PROJECT_ROOT/build" ]; then
    mkdir "$PROJECT_ROOT/build"
fi

# 清空 build 目录内容
rm -rf "$PROJECT_ROOT/build"/*

# 进入 build 目录并编译
cd "$PROJECT_ROOT/build"
cmake ..
make

# 回到项目根目录
cd "$PROJECT_ROOT"

# 安装头文件
if [ ! -d /usr/include/mymuduo ]; then
    mkdir /usr/include/mymuduo
fi

for header in *.h; do
    cp "$header" /usr/include/mymuduo/
done

# 安装动态库
sudo cp "$(pwd)/lib/libmymuduo.so" /usr/lib/

# 刷新动态库缓存
sudo ldconfig
