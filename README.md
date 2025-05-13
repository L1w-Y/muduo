# muduo（更新中）
## Channel类
设计思想

作用

代码细节

## Poller类
作用

设计方法，静态工厂

抽象接口

派生类构造

公共的DefaultPoller.cc

## eventloop

三个类之间的协作

线程间通信

共享资源？每个thread 一个loop

## Thread EventLoopThread EventLoopThreadPool

EventLoopThread的逻辑是
构造时将thread_(std::bind(&EventLoopThread::threadFunc,this),name)绑定，作为thread类的func_回调在调用startLoop时，调用thread_.start()，创建子线程用智能指针管理，然后子线程内部又调用func_（），也就是EventLoopThread的threadFunc()方法，在这个方法内创建了一个loop,调用外部传给的EventLoopThread的callback_，然后在这个threadFunc()回调中执行loop.loop();
两个问题
1.设计成这样复杂的回调逻辑是为什么
2.thread_(std::bind(&EventLoopThread::threadFunc,this),name)2.thread_(std::bind(&EventLoopThread::threadFunc,this),name)将类内部成员方法传给thread_，但是构造时是move,这样内成员方法不就是脱离对象实例在调用吗