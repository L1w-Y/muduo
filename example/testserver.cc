#include<mymuduo/Tcpserver.h>
#include<mymuduo/Logger.h>


#include<string>

class EchoServer{
public:
    EchoServer(EventLoop *loop,
            const InetAddress &addr,
            const std::string &name)
            :server_(loop,addr,name)
            ,loop_(loop)
            {
                server_.setNewConnectionCallback([this](const TcpConnectionPtr &conn){
                    this->onConnection(conn);
                });

                server_.setMessageCallback([this](const TcpConnectionPtr &conn,Buffer *buf, Timestamp time){
                    this->onMessage(conn,buf,time);
                });

                server_.setThreadNum(3);
            }
    void start(){
        server_.start();
    }
private:
    void onConnection(const TcpConnectionPtr &conn){
            if(conn->connected()){
                LOG_INFO("Conn Up: %s",conn->peerAddress().toIpPort().c_str());
            }else{
                LOG_INFO("Conn Down: %s",conn->peerAddress().toIpPort().c_str());
            }
    }

    void onMessage(const TcpConnectionPtr &conn,Buffer *buf, Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown();
    }

    EventLoop *loop_;
    TcpServer server_;
};

int main(){
    EventLoop loop;
    InetAddress addr(8000);
    EchoServer server(&loop,addr,"EchoServer-01");
    server.start();
    loop.loop();

    return 0;
}