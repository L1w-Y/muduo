cmake_minimum_required(VERSION 2.5)
project(mymuduo)

# mymudou最终编译成so动态库,设置动态库的路径
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_CXX_STANDARD 14) 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

#定义参与编译的源文件
aux_source_directory(. SRC_LIST)    
#编译动态库
add_library(mymuduo SHARED ${SRC_LIST})