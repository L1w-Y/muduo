#include "Poller.h"
#include <stdlib.h>

Poller* Poller::newDefaultPoller(EventLoop *loop){
    if(::getenv("MUDUO_USE_POLL")){
        //生成poll实例
    }
    else{
        //生成epoll实例
    }
}