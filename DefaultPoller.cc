#include "Poller.h"
#include <stdlib.h>
#include"EPollPoller.h"
Poller* Poller::newDefaultPoller(EventLoop *loop){
    return new EPollPoller(loop);
}