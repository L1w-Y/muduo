#include"Logger.h"
#include"iostream"
#include"Timestamp.h"
Logger& Logger::instance(){
    static Logger Logger;
    return Logger;
}
void Logger::setLogLevel(int level){
        LogLevel_=level;
}

void Logger::Log(std::string msg){
    switch (LogLevel_)
    {
    case INFO:
        std::cout<<"[INFO]";
        break;
    case ERROR:
        std::cout<<"[ERROR]";
        break;
    case FATAL:
        std::cout<<"[FATAL]";
        break;
    case DEBUG:
        std::cout<<"[DEBUG]";
        break;            
    default:
        break;
    }
    std::cout << msg; // 打印消息
    std::cout << ":" << Timestamp::now().toString() << std::endl;
    std::cout<<"print time"<<":"<<Timestamp::now().toString()<<std::endl;
}