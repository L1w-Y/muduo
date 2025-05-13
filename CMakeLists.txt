cmake_minimum_required(VERSION 2.5)
project(mymudou)

# mymudou最终编译成so动态库,设置动态库的路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_CXX_STANDARD 14) 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

#定义参与编译的源文件
aux_source_directory(. SRC_LIST)    
#编译动态库
add_library(mymudou SHARED ${SRC_LIST})