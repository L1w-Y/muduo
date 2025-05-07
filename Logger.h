#pragma once
#include"noncopyable.h"
#include<string>
/*日志级别
INFO  普通日志输出
ERROR 不影响正常运行的错误
FATAL 重要bug
DEBUG 调试信息
*/
enum LogLevel{
    INFO,
    ERROR,
    FATAL,
    DEBUG,
};

#define LOG_INFO(logmsgFormat, ...)\
do \
{\
    logger &Logger = Logger::instance();\
    logger.setLogLevel(INFO);\
    char bu[1024]={};\
    snprintf(buf,1024,logmsgFormat,##__VA_ARGS__);\
    logger.log(buf);\
}while(0)

#define LOG_ERROR(logmsgFormat, ...)\
do \
{\
    logger &Logger = Logger::instance();\
    logger.setLogLevel(ERROR);\
    char bu[1024]={};\
    snprintf(buf,1024,logmsgFormat,##__VA_ARGS__);\
    logger.log(buf);\
}while(0)

#define LOG_FATAL(logmsgFormat, ...)\
do \
{\
    logger &Logger = Logger::instance();\
    logger.setLogLevel(FATAL);\
    char bu[1024]={};\
    snprintf(buf,1024,logmsgFormat,##__VA_ARGS__);\
    logger.log(buf);\
}while(0)

#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...)\
do \
{\
    logger &Logger = Logger::instance();\
    logger.setLogLevel(DEBUG);\
    char bu[1024]={};\
    snprintf(buf,1024,logmsgFormat,##__VA_ARGS__);\
    logger.log(buf);\
}while(0)
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif

class Logger : noncopyable{
public:
    static Logger& instance();
    void setLogLevel(int level);
    void Log(std::string);
private:
    Logger();
    int LogLevel_{};
};