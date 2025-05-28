# 万字剖析muduo高性能网络库设计细节

Muduo 是一个由陈硕大神开发的 Linux 服务器端高性能网络库。网络库核心框架采用基于同步非阻塞IO的multi-reactor模型，并且遵循one loop per thread的设计理念，本项目对muduo库核心部分进行重构，采用c++11的特性，移除muduo对boost库的依赖。

我会先从整体的角度对muduo库设计进行阐述，再深入细节，对每个模块的架构，设计思想，各模块协助流程，编程细节进行深入解析。

本人出于个人学习的目的写下这篇文章，如有讲解错误的地方，欢迎提出，一起交流，下面进入正题。

---

## 📚 目录
- [一、前置知识](#一前置知识)
  - [1.1 阻塞/非阻塞 与 同步/异步](#11-阻塞非阻塞-与-同步异步)
      - [系统调用层面](#1-io系统调用层面)
      - [应用层面](#2-应用层面)
  - [1.2 五种IO模型](#12-五种IO模型)
      - [阻塞IO](#🟢阻塞IO)
      - [非阻塞IO](#🟢非阻塞IO)
      - [IO多路复用](#🟢IO-多路复用)
        - [Select](#🟡select)
        - [Poll](#🟡poll)
        - [epoll](#🟡epoll)
      - [信号驱动](#🟢信号驱动)
      - [异步](#🟢异步)
- [二、muduo库概述](#二muduo库概述)
  - [2.1 Reactor模型](#21-reactor模型)
  - [2.2 muduo库核心架构](#22-muduo库核心架构)
- [三、辅助模块](#三辅助模块)
  - [3.1 noncopyable](#31-noncopyable)
  - [3.2 Logger日志模块](#32-logger-日志模块)
  - [3.3 buffer缓冲区](#33-buffer-缓冲区)  
- [四、multi-Reactor事件循环模块](#四multi-reactor事件循环模块)
- [五、线程池模块](#五线程池模块)
- [六、Tcp通信模块](#六tcp通信模块)
- [七、模块间通信](#七模块间通信)
- [八、工作流程](#八工作流程)
- [九、总结](#九总结)
- [参考](#参考文章)

## 一、前置知识

在对muduo库进行正式剖析之前，先聊一聊网络编程中一些重要的基本概念。

### 1.1 阻塞/非阻塞 与 同步/异步

`阻塞/非阻塞` 和 `同步/异步` 是两个不同维度的概念，我们分别在系统调用和应用层面上进行讨论：

---

### 1. IO系统调用层面

可以说正是因为底层 I/O 系统调用的行为（如`read`, `write`）存在这两种维度，才构成了上层进程通信模型的不同概念

#### - 🟢**Blocking vs Non-blocking**

描述的是**单个系统调用是否会使进程挂起**，以下以 `read()` 为例：

- **Blocking I/O**：  
  当调用 `read()` 后，如果内核缓冲区没有数据，内核会让该进程从「运行态」进入「睡眠态」。CPU 会调度执行其他进程。直到数据准备好并被复制到用户空间缓冲区后，内核才会唤醒该进程，`read()` 调用才会返回。这个过程中，进程是被阻塞的。

- **Non-blocking I/O**：  
  通过设置文件描述符的 `O_NONBLOCK` 标志实现。当用户进程调用 `read()` 时，如果内核缓冲区没有数据，系统调用会立即返回错误码。进程不会被阻塞，可以继续执行其他代码。通常用户会通过轮询方式在稍后再次尝试调用 `read()`。

#### -🟢**Synchronous vs Asynchronous**

描述的是**整个 I/O 操作（从发起请求到数据最终可用）是由谁来完成的**，以及结果是如何被通知给进程的。

- **Synchronous I/O**：  
  在同步 I/O 中，用户进程是发起 I/O 操作的主体，并且需要主动等待或查询 I/O 操作的结果。
  
  **🌟🌟在处理 IO 的时候，阻塞和非阻塞都是同步 IO。只有使用了特殊的 API 才是异步 IO🌟🌟** 
  - 阻塞 I/O 是同步的，因为进程通过阻塞来“等待”结果。
  - 非阻塞 I/O 也是同步的，因为进程需要通过反复轮询（polling）来“查询”操作是否完成。
  - I/O 多路复用（如 `select`, `epoll`）同样是同步的。虽然它们可以同时监听多个文件描述符，看起来像“异步”，但本质上 `select/epoll` 调用会阻塞进程以“等待”事件就绪。而且当事件返回后，数据还在内核缓冲区，用户进程仍需调用 `read()` 才能完成数据读取。因此整个 I/O 操作仍然是由用户主动完成的。

- **Asynchronous I/O**：  
  异步 I/O（如 `aio_read()`）中，用户进程发起请求后立即返回，可以做其他事。  
  内核会独立完成两个阶段：

  1. 等待数据准备好；
  2. 将数据从内核缓冲区拷贝到用户指定缓冲区。

  当这两个阶段完成后，内核通过 **信号** 或 **回调函数** 的方式通知用户进程：**数据已准备完毕且拷贝完成**。
  
  也就是说在异步io中，数据的读写是由内核帮助用户完成的。

---

### 2. 应用层面

在应用层面，这些概念更多地关注程序如何组织逻辑、等待结果、以及响应 I/O 的方式。

#### - 🟢**Blocking vs Non-blocking**

当你调用某个函数（通常是 I/O 相关，比如读取 socket、发请求等）时，当前线程是否被挂起等待结果返回。

*   **Blocking I/O**：
    当应用程序执行一个操作（比如调用一个函数去获取远程 API 数据，或查询数据库）时，如果该操作需要一些时间来完成，应用程序的当前执行线程会等待该操作返回结果后才能继续执行后续代码。
    如果某个任务耗时较长，整个应用（或至少是该线程）会显得 "卡顿" 或无响应。

    **例子**：一个传统的单线程 Web 服务器，当它为一个请求查询数据库时，它必须等待数据库返回结果，在此期间无法处理其他新的请求。

*   **Non-blocking I/O**：
    当应用程序执行一个操作时，该操作会立即返回，而不会等待其真正完成。应用程序可以继续执行其他任务。
    应用程序不会因为等待某个操作而被挂起，通常需要一种机制来稍后获取操作的结果，比如通过轮询检查状态，或者注册一个回调函数。

#### - 🟢**Synchronous vs Asynchronous**

在应用层面的同步和异步更多是描述 **整个操作或者业务逻辑的完成过程，是由调用方主动去完成，还是系统或框架异步地帮你完成后，通知你结果**。

*   **Synchronous I/O**：
    整个流程是顺序的，即使底层使用了非阻塞 I/O，如果应用逻辑设计成 "发起 -> 等待结果 -> 处理结果" 的串行模式，那么从应用行为上看它仍然是同步的。

*   **Asynchronous I/O**：
    在应用层面的一个处理逻辑，比如 A 向 B 请求调用一个网络 I/O 接口时（或者调用某个业务逻辑 API 接口时），向 B 传入请求的事件以及事件发生时通知的方式，A 就可以处理其它逻辑了。
    当 B 监听到事件处理完成后，会用事先约定好的通知方式，通知 A 处理结果。

    基于 reactor 的高性能网络框架多是采用这样的非阻塞 + 异步的设计模式，开发者注册相应事件和对应事件发生的回调函数，在监听到相应事件发生时，网络框架主动调用回调函数。
    虽然在网络库内部的事件监听和 I/O 读写操作时，采用的系统接口比如 `epoll_wait` 和 `recv()` 等等仍是同步的过程，但在上层应用层面的事件注册和处理逻辑是在异步执行。

---

### 1.2 五种IO模型

---

#### 🟢**阻塞IO**
在所示的这样一个传统的socket流程中，服务器端代码阻塞在了accept()和read()两个函数上
![阻塞IO](res/阻塞io流程.gif)

再看read()函数阻塞的内部流程,其实发生了两次阻塞

![read函数](res/read流程.png)
![read函数](res/read函数内部.gif)

这种模式有相当大的弊端，如果对端一直不发数据，那么服务端线程将会一直阻塞在 read 函数上死等，无法进行任何其他的操作。

---

#### 🟢**非阻塞IO**
非阻塞模式是通过操作系统提供的api，将文件描述符设置为非阻塞状态，比如：
```cpp
fcntl(connfd, F_SETFL, O_NONBLOCK);
int n = read(connfd, buffer) != SUCCESS);
```
这种状态下的read函数在调用够后会直接返回，不管数据是否准备号，用户通过返回值的不同类型来自行判断，通常用户层面采用反复调用的方式来配合调用，如图：

![read函数](res/非阻塞read.png)
![read函数](res/非阻塞read.gif)

可以看到这种系统调用不会挂住程序，避免线程阻塞。

但是这里要注意，在数据还未到达网卡，或者到达网卡但还没有拷贝到内核缓冲区之前，这个阶段是非阻塞的。当数据已到达内核缓冲区，**此时调用 read 函数仍然是阻塞的**，需要等待数据从内核缓冲区拷贝到用户缓冲区，才能返回。

非阻塞 I/O 本身不是缺点，但是单纯的非阻塞 I/O 上需要搭配反复轮询去资源是否就绪，就会造成CPU空转，造成资源浪费，可以认为是非阻塞 I/O 的一种使用方式上的缺陷。

---
#### 🟢**IO 多路复用**
前面两种情况都是一次监听一个文件描述符，而IO多路复用就是：

**🌟同时监听多个文件描述符，并在有一个或多个就绪（可读/可写/异常）时，通知用户进行处理的机制🌟**
  #### - 🟡**select**
  select 是操作系统提供的系统调用函数，通过它，我们可以把一个文件描述符的数组发给操作系统， 让操作系统去遍历，确定哪个文件描述符可以读写， 然后告诉我们去处理：

  ![select](res/select_1.gif)

  通常采用select设计如下：
  ```cpp
  //一个线程不断接受客户端连接，并把 socket 文件描述符放到一个 list 里
  while(1) {
  connfd = accept(listenfd);
  fcntl(connfd, F_SETFL, O_NONBLOCK);
  fdlist.add(connfd);
  }

  //然后另起一个线程调用 select，将这批文件描述符 list 交给操作系统去遍历
  while(1) {
  /*
  把一堆文件描述符 list 传给 select 函数
  有已就绪的文件描述符就返回，nready 表示有多少个就绪的
  */
  nready = select(list);
  ...
  ...
  }
  ```
  但是在select返回后，操作系统会将准备就绪的文件描述符做上标识，用户其实还需要自己对文件描述符数组进行遍历，来找到有事件发生的fd。

  select有几个细节：
  1. select 调用需要传入 fd 数组，需要拷贝一份到内核，高并发场景下这样的拷贝消耗的资源是惊人的
  2. select 在内核层仍然是通过遍历的方式检查文件描述符的就绪状态，是个同步过程，只不过无系统调用切换上下文的开销，当连接数量很多时，这个轮询的成本变成 O(n)
   
  3. select 仅仅返回可读文件描述符的个数，具体哪个可读还是要用户自己遍历。（可优化为只返回给用户就绪的文件描述符，无需用户做无效的遍历）

  #### - 🟡**poll**
  poll对select进行了优化，除去了 select 只能监听 1024 个文件描述符的限制

  #### - 🟡**epoll**

  最后的epoll针对上述select中提到的几个细节进行了优化：
  1. 文件描述符集合采用红黑树的结构保存在内核中，无需用户每次都重新传入，只需告诉内核修改的部分即可
  2. 内核不再通过轮询的方式找到就绪的文件描述符，而是通过异步 IO 事件唤醒
  3. 内核仅会将有 IO 事件的文件描述符返回给用户，用户也无需遍历整个文件描述符集合 
  
  #### **🌟`epoll`的机制：基于`事件驱动+回调`🌟**
  
  通常epoll的三步使用流程是：
  1. 注册事件，只需要注册一次，通过epoll_ctl()向存在内核的epoll红黑树上挂fd和对应的回调函数指针
  ```cpp
  //告诉内核：“我关心哪个 fd 上的哪个事件，比如 EPOLLIN（可读）。”
  epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event);
  ```
  2. 等待事件就绪（阻塞状态） 
  
  内核会在你注册的 fd 上监听事件,一旦有事件发生，会立即“唤醒”你。
  ```cpp
  int n = epoll_wait(epfd, events, MAX_EVENTS, timeout);
  ```
  3. 只处理就绪 fd

  所以epoll不再对fd进行轮询

  - epoll 内核里使用了红黑树 + 就绪链表来管理监听 fd；
  - 当你调用 epoll_ctl 注册 fd 时，fd 被挂到内核维护的数据结构中；
  - 当某个 fd 状态变化（如收到数据），内核通过中断或回调直接把这个 fd 放入就绪列表；
  - epoll_wait 在睡眠中被内核事件驱动唤醒，只返回已经就绪的 fd。

  ✅ 你监听了 10000 个连接，如果只 3 个就绪，epoll_wait 只返回那 3 个，无需你自己轮询。

  过程如下：

  ![select](res/epoll.gif)

---


所以在io多路复用中，整体的流程是这样的：

![select](res/io多路复用.png)

---

####  🟢**信号驱动**

信号驱动的核心思想是：

**利用内核信号机制，在 I/O 就绪时发送一个信号通知进程，然后再由进程读取数据。**
换句话说就是
* 程序不需要一直轮询检查 fd 是否可读，
* 而是内核主动 发一个 SIGIO 信号，通知你：“数据来了”，
* 程序在信号处理函数中再去调用 read() 等函数进行读写操作。

![select](res/信号驱动.png)

内核在第一个阶段是异步的，第二个阶段是同步的；与非阻塞IO的区别在于它提供了消息通知机制，不需要用户进程不断的轮询检查，减少了系统API的调用次数，提高了效率

---

####  🟢**异步**
应用程序发起 I/O 请求后，立即返回，不需要等待内核完成操作；当内核完成 I/O 操作后，主动通知应用程序（比如通过回调或信号），并附带读/写的结果。

 与非阻塞 I/O 不同：非阻塞 I/O 仍然要自己调用 read()/write()，而 AIO 是由内核完成操作并返回结果，真正的“交给内核全权处理”

![select](res/异步.png)

像nginx，Netty底层采用的就是操作系统提供的异步io接口实现的网络库

---

<p align="right"><a href="#万字剖析muduo高性能网络库设计细节">回到顶部⬆️</a></p>

## 二、muduo库概述

### 2.1 reactor模型

reactor是这样一种模式，它要求主线程只负责监听fd上是否有事件发生，有事件发生时就通知工作子线程来处理，而主线程只做监听，其他什么都不做，接收新的连接，处理请求，读写数据都在工作线程中完成。

---

假设一个简单的 HTTP GET 请求处理流程：

*   服务器已启动，主线程创建了 `epoll` 实例 (`epoll_fd`)。
*   一个客户端已连接到服务器，服务器接受连接后得到一个客户端套接字文件描述符 `client_socket_fd`。

🟢步骤 1： 主线程注册读就绪事件

主线程往 `epoll` 内核事件表中注册 `client_socket_fd` 上的读就绪事件。

*   **动作**：主线程调用 `epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket_fd, {events: EPOLLIN})`。
*   **目的**：告诉内核：“请帮我监听 `client_socket_fd`，如果它的内核接收缓冲区里有数据了（即客户端发数据过来了），就通知我。”

🟢步骤 2： 主线程等待事件

主线程调用 `epoll_wait` 等待 `client_socket_fd` (或其他已注册的fd)上有事件发生。

*   **动作**：主线程调用 `epoll_wait(epoll_fd, &events, max_events, timeout)`。此时主线程会阻塞（或在超时后返回）。
*   **例子**：服务器现在空闲，等待客户端发送 HTTP 请求。

🟢步骤 3：读就绪事件发生，主线程分发任务

当 `client_socket_fd` 上有数据可读时，`epoll_wait` 通知主线程。主线程则将 `client_socket_fd` 可读事件放入请求队列。

*   **客户端行为**：客户端发送一个 HTTP GET 请求，例如：
    ```
    GET /hello HTTP/1.1
    Host: example.com
    Connection: keep-alive
    User-Agent: Mozilla/5.0 ...

    ```
*   **内核行为**：这段数据到达服务器内核，被放入 `client_socket_fd` 的接收缓冲区。
*   **`epoll_wait` 返回**：由于 `client_socket_fd` 的接收缓冲区不再为空（发生了**读就绪事件** `EPOLLIN`），`epoll_wait` 从阻塞状态返回，并告诉主线程 `client_socket_fd` 现在可读。
*   **主线程动作**：主线程将一个表示“`client_socket_fd` 可读”的任务（例如一个包含 `client_socket_fd` 和事件类型 `EPOLLIN` 的结构体）放入一个共享的请求队列中。

🟢步骤 4：工作线程处理读事件及业务逻辑，并注册写事件

睡眠在请求队列上的某个工作线程被唤醒，它从 `client_socket_fd` 读取数据，处理客户请求，然后往 `epoll` 内核事件表中注册该 `client_socket_fd` 上的写就绪事件。

*   **工作线程动作 (读取)**：一个空闲的工作线程从请求队列中取出任务，发现是 `client_socket_fd` 的读事件。它调用 `recv(client_socket_fd, buffer, buffer_size, 0)`。
    *   **例子**：`buffer` 中现在读入了客户端发送的 HTTP 请求数据。
*   **工作线程动作 (处理)**：工作线程解析 HTTP 请求。假设它确定要返回一个简单的 "Hello World" 响应。它准备好了响应数据：
    ```
    HTTP/1.1 200 OK
    Content-Type: text/plain
    Content-Length: 12

    Hello World!
    ```
*   **工作线程动作 (注册写事件)**：现在工作线程有数据要发送回客户端了。它需要确保 `client_socket_fd` 的内核发送缓冲区有空间。所以它调用 `epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_socket_fd, {events: EPOLLIN | EPOLLOUT})` (修改监听事件，在原来的 `EPOLLIN` 基础上增加 `EPOLLOUT`。如果此时不再关心客户端是否会立即发送更多数据，也可以只设置为 `EPOLLOUT`)。
*   **目的**：告诉内核：“请帮我监听 `client_socket_fd`，如果它的内核发送缓冲区有足够空间可以让我写入数据了，就通知主线程。”

🟢步骤 5：主线程再次等待事件

主线程（可能在处理完其他事件后再次）调用 `epoll_wait` 等待 `client_socket_fd` (或其他fd)可写。

*   **动作**：主线程调用 `epoll_wait(epoll_fd, ...)`。
*   **注意**：此时 `epoll_wait` 也会继续监听其他已注册 fd 上的读/写事件。

🟢步骤 6：写就绪事件发生，主线程分发任务

当 `client_socket_fd` 可写时，`epoll_wait` 通知主线程。主线程将 `client_socket_fd` 可写事件放入请求队列。

*   **内核行为**：通常情况下，socket 的发送缓冲区在大部分时间都是有可用空间的（除非网络非常拥堵，或者对端接收非常慢，导致发送缓冲区被填满）。所以，很可能在工作线程注册 `EPOLLOUT` 后，`client_socket_fd` 立刻就是可写的。
*   **`epoll_wait` 返回**：`epoll_wait` 检测到 `client_socket_fd` 的发送缓冲区有空间（发生了**写就绪事件** `EPOLLOUT`），于是返回，并通知主线程 `client_socket_fd` 现在可写。
*   **主线程动作**：主线程将一个表示“`client_socket_fd` 可写”的任务放入请求队列。

🟢步骤 7：工作线程处理写事件

睡眠在请求队列上的某个工作线程被唤醒，它往 `client_socket_fd` 上写入服务器处理客户请求的结果。

*   **工作线程动作 (写入)**：一个工作线程（可能是同一个，也可能是另一个）从请求队列中取出任务，发现是 `client_socket_fd` 的写事件。它调用 `send(client_socket_fd, response_data, strlen(response_data), 0)`。

*   **后续**：发送完数据后，如果工作线程没有更多数据要立即发送给这个客户端（例如，响应已经完整发送），它通常会通过 `epoll_ctl` 修改 `client_socket_fd` 的监听事件，移除 `EPOLLOUT`，以避免 `epoll_wait` 因为发送缓冲区持续可写而不断触发不必要的写就绪通知（这被称为 "busy-looping" 或 "level-triggered storm"）。它可能会重新只监听 `EPOLLIN`，等待客户端的下一个请求（如果连接是持久的，如 HTTP Keep-Alive）。如果是一次性短连接，可能会准备关闭连接。

---

![Reactor](res/reactor.png)

![Reactor](res/muduo模型.png)

---

### 2.2 muduo库核心架构

Muduo 的核心设计采用multi-reactor模型， 是`one loop per thread + thread pool 的 Reactor 变体`

1. **主 Reactor(base-loop)**
  - 当一个Tcp服务（TcpServer）启动后，并且创建一个base-loop和一个线程池管理sub-loop
  - 主 Reactor运行在主线程中，拥有一个 baseLoop。
  - 它的核心职责是监听新的客户端连接请求 (通过 Acceptor 组件)，和分配新连接。
  - 当有新的连接到达时，主 Reactor 接受 (accept) 这个连接。
  - 主 Reactor 在接受新连接后，并不会自己处理这个连接上的后续 I/O 事件（如读写数据）。
  - 主 Reactor 会将这个新创建的连接（TcpConnection 对象）分发给一个从 Reactor。分发策略可以是轮询或其他负载均衡算法。
  
2. **从 Reactors (sub-loop)**

  - 通常由一个线程池管理，每个线程拥有唯一一个属于自己的EventLoop（one loop per thread）
  - 子loop的数量通常是根据内核数量分配
  - 一旦一个 TcpConnection 被分配给了一个特定的从 Reactor，那么该连接上的所有后续 I/O 事件（数据可读、可写、连接关闭等）都将由这个从 Reactor 在其所属的 I/O 线程中处理。


![muduo](res/multi架构.jpg)


我主要将muduo库分为四大模块：


- **📌辅助模块**
- **📌multi-Reactor事件循环模块**
- **📌线程池模块**
- **📌Tcp通信模块**


在深入muduo库源码时，会发现里面有相当灵活且复杂的回调机制，在不同类不同模块之间传递，这使得muduo库中各个模块充分解耦，职责分明，但相互之间又紧密协同，只有理清各个模块的架构和工作过程，才能真正一探muduo库的设计思想。

<p align="right"><a href="#万字剖析muduo高性能网络库设计细节">回到顶部⬆️</a></p>

## 三、辅助模块

### 3.1 noncopyable

直接继承这样一个抽象的禁止拷贝和赋值基类，减少重复书写
- 构造和析构设计为protected，使基类无法被直接实例化，但允许派生类正常构造和析构
```cpp
class noncopyable{
public:
    noncopyable(const noncopyable&)=delete;
    noncopyable& operator=(const noncopyable&)=delete;
protected:
    noncopyable()=default;
    ~noncopyable()=default;
};
```
---

### 3.2 Logger 日志模块

定义了四种日志级别：
- INFO：普通日志输出
- ERROR：记录不影响程序运行的错误
- FATAL：记录重要错误，程序会因此退出
- DEBUG：调试信息，仅在调试模式下生效
```cpp
enum LogLevel{
    INFO,
    ERROR,
    FATAL,
    DEBUG,
};
```
- Logger类结构如下：
```cpp
class Logger : noncopyable{
public:
    static Logger& instance();
    void setLogLevel(int level);
    void Log(std::string);
private:
    Logger()=default;
    int LogLevel_{};
};
```
- `static Logger& instance():`
```cpp
Logger& Logger::instance(){
    static Logger Logger;
    return Logger;
}
```
获取 Logger 单例,使用静态局部变量确保只创建一个实例
- `void setLogLevel(int level):`
```cpp
void Logger::setLogLevel(int level){
    LogLevel_=level;
}
```
公用接口，用于设置当前日志实例的日志级别。每次调用日志宏时，都会先调用此方法设置对应的级别。

- `void Log(std::string msg):`
```cpp
void Logger::Log(std::string msg){
    // 根据 LogLevel_ 输出级别前缀
    switch (LogLevel_)
    {
    case INFO:  std::cout<<"[INFO]";  break;
    case ERROR: std::cout<<"[ERROR]"; break;
    case FATAL: std::cout<<"[FATAL]"; break;
    case DEBUG: std::cout<<"[DEBUG]"; break;
    default: break;
    }
    // 打印实际消息和时间戳
    std::cout << msg;
    std::cout << ":" << Timestamp::now().toString() << std::endl;
}
```

这是实际执行日志输出的方法。它会根据当前设置的 LogLevel_ 输出对应的级别标签（如 [INFO]），然后输出用户提供的消息和时间戳。

- `日志宏 (LOG_INFO, LOG_ERROR, LOG_FATAL, LOG_DEBUG): 这些宏是用户与日志系统交互的主要方式。以 LOG_INFO 为例：`

```cpp
#define LOG_INFO(logmsgFormat, ...) \
    do{ \
        Logger &logger = Logger::instance(); \       // 1. 获取 Logger 单例
        logger.setLogLevel(INFO); \                 // 2. 设置当前日志级别为 INFO
        char buf[1024]={}; \                       // 3. 创建一个缓冲区
        snprintf(buf,1024,logmsgFormat,##__VA_ARGS__); \ // 4. 使用 snprintf 格式化日志消息
        logger.Log(buf); \                          // 5. 调用 Log 方法输出
    }while(0)
```

---

### 3.3 Buffer 缓冲区
这个类的设计是提供一个灵活且高效的内存缓冲区，在tcp连接中收发消息提供一个灵活的缓冲区

设计结构如下：
![buffer](res/buffer.png)

这个缓冲区在概念上被划分为三个主要部分：

1. 头部预留字节（8字节）
   - 这是缓冲区开头预留的一块空间
   - 主要目的是让你能够高效地在现有内容之前添加数据。这在网络协议中非常常见，比如你可能收到了数据负载，然后需要在前面加上长度字段或协议头
   - KCheapPrepend 常量定义了这个区域的初始大小
2. 可读字节空间
   - 这是缓冲区中实际存储的、准备好被读取或处理的数据
   - readerIndex_ 指向这部分数据的开始
   - writeIndex_ 指向这部分数据的结束，同时也是可写字节空间的开始位置
   - 如果读空间有数据，那么下一次写入数据将是紧接着数据末尾
3. 可写字节空间（从 writeIndex_ 到 buffer_.size() - 1）
   - 这是缓冲区末尾的空闲空间，新数据被写入到这个位置
   - writeIndex_ 指向这个区域的开始

- `一些功能性的对外接口`
```cpp
// 返回可读字节数（写指针 - 读指针）
    size_t readableBytes() const {
        return writeIndex_ - readerIndex_;
    }

    // 返回可写字节数（缓冲区总大小 - 写指针）
    size_t writableBytes() const {
        return buffer_.size() - writeIndex_;
    }

    // 返回前置空间的大小（读指针之前的区域,包含头部）
    size_t prependableBytes() const {
        return readerIndex_;
    }

    char* beginWrite(){
        return begin() + writeIndex_;
    }
    const char* beginWrite() const {
        return begin() + writeIndex_;
    }
```
- `读取数据`
```cpp
    const char* peek() const {
        return begin() + readerIndex_;
    }

    // 读取所有可读数据并以 std::string 返回，同时清空缓冲区
    std::string retrieveAllAsString() {
        return retrieveAsString(readableBytes());
    }

    // 读取 len 长度的数据并返回为 string，同时推进读指针
    std::string retrieveAsString(size_t len) {
        std::string result(peek(), len); // 从 peek 开始复制 len 字节
        retrieve(len);                   // 读取后移动读指针
        return result;
    }

        // 读取 len 长度的数据，相当于向前推进 readerIndex_
    void retrieve(size_t len) {
        if (len < readableBytes()) {
            // 只推进部分读指针
            readerIndex_ += len;
        } else {
            // 如果要读的长度超过了可读数据，则全部清空
            retrieveAll();
        }
    }

    // 清空所有数据，重置读写指针
    void retrieveAll() {
        readerIndex_ = writeIndex_ = KCheapPrepend;
    }
```
- `扩容思路和方法`
```cpp
    
    void append(const char *data,size_t len){
        ensureWriteableBytes(len);
        std::copy(data,data+len,beginWrite());
        writeIndex_ += len;
    }

    void ensureWriteableBytes(size_t len){
        if(writableBytes()<len){
            makeSpace(len);
        }
    }
    void makeSpace(size_t len){
    if(writableBytes()+prependableBytes() < len + KCheapPrepend){
        buffer_.resize(writeIndex_+len);
    }else{
        size_t readable = readableBytes();
        std::copy(begin() + readerIndex_, begin()+writeIndex_, begin()+KCheapPrepend);
        readerIndex_=KCheapPrepend;
        writeIndex_= readerIndex_+readable;
      }
    }
```
在每次插入数据之前，调用ensureWriteableBytes接口确保buffer缓冲区有足够的写入空间，如果不够，就调用makeSpace方法进行扩容

扩容前首先进行判断：`if(writableBytes()+prependableBytes() < len + KCheapPrepend)`
表示：可写入的空间+ read指针前的空间**是否大于**写入数据长度+len
1. `否`
   那就直接resize()开辟空间
2. `是` 
    代表：可读数据中头部有部分数据已经被读走，前面空出的空间+剩余的可写空间已经够大，就不用重新开辟
    将剩余的未读数据移动到头部，然后将可用空间拼接起来再利用，如图：
    ![扩容](res/扩容.png)

`🟢缓冲区读操作`

从一个给定的 fd 中读取数据，将其存入 Buffer 中

```cpp
    ssize_t Buffer::readFd(int fd, int* savedErrno) {
    // 在栈上创建一个临时缓冲区，大小为 65536 字节 (64KB)
    char extrabuf[65536] = {}; 

    // 定义一个 iovec 结构体数组，用于 readv 系统调用
    // iovec 结构体用于描述一块内存区域
    // readv 可以一次性从文件描述符读取数据到多个不连续的内存区域
    struct iovec vec[2];

    // 获取当前 Buffer 中可写字节数
    const size_t writable = writableBytes();

    // 设置第一个 iovec 结构体：指向 Buffer 内部的可写空间
    vec[0].iov_base = begin() + writeIndex_; // 可写区域的起始地址
    vec[0].iov_len = writable;              // 可写区域的长度

    // 设置第二个 iovec 结构体：指向栈上的临时缓冲区 extrabuf
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;       // extrabuf 的总大小
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;

    // 调用 readv 系统调用从 fd 读取数据到 vec 指定的内存区域
    // readv 会先尝试填满 vec[0]，如果还有数据且iovcnt是2，再尝试填满 vec[1]
    const ssize_t n = ::readv(fd, vec, iovcnt);

    // 处理 readv 的返回值
    if (n < 0) {
        // 读取出错，保存错误码
        *savedErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        // 读取成功，并且所有读取到的数据都成功存入了 Buffer 内部的可写空间 (vec[0])
        // 直接增加写指针 writeIndex_
        writeIndex_ += n;
    } else {
        // 读取成功，但读取到的数据量 n 超过了 Buffer 内部初始的可写空间 writable
        // 这意味着数据一部分存入了 vec[0]，剩下的存入了 extrabuf (vec[1])
        // 首先，Buffer 内部的可写空间已全部被填满
        writeIndex_ = buffer_.size(); // 将写指针移到 Buffer 的末尾
        // 然后，将 extrabuf 中超出 writable 部分的数据追加到 Buffer 中
        // n - writable 是实际存储在 extrabuf 中的数据量
        // append 函数会负责处理 Buffer 空间的扩展
        append(extrabuf, n - writable);
    }
    return n;
}

```

**使用了 readv 和 struct iovec 结合实现一次性从文件描述符读取数据到多个不连续的内存块中**
1. 在栈上声明一个字符数组，大小为 65536 字节（64KB），用来当作缓冲区的缓冲区
2. struct iovec vec[2]
   - iovec 结构体数组，用于 readv 和 writev 系统调用，它定义了一个内存区域（内存起始地址和长度）。iovec 结构体包含两个元素：
   ```cpp
    struct iovec {
    void  *iov_base;    /* Starting address */
    size_t iov_len;     /* Number of bytes to transfer */
        };
3. 把数组第一块内存定义为buffer的写缓冲区，第二块内存设置为栈上缓冲区exterbuf
4. **🌟const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;🌟**
   - 决定 readv 调用中 iovec 数组的实际使用数量 (iovcnt)
   - 逻辑：
     - 如果 Buffer 的可写空间 writable 小于 extrabuf 的大小（64KB），那么我们同时使用 Buffer 的内部空间和 extrabuf (即 iovcnt = 2)。这样做的目的是，如果读取的数据很多，先填满 Buffer 的可写部分，多余的再放入 extrabuf。 之后再调用append把extrabuf的内容写入可写空间中，这个时候会对buffer进行一次扩容（调用makeSpace）
    
     - 如果 Buffer 的可写空间 writable 大于等于 extrabuf 的大小，那么只使用 Buffer 的内部空间 (iovcnt = 1)。因为即使 extrabuf 能提供的空间（64KB）都填满了，Buffer 自身也还有空间。**这个情况就是一次读的数据保证大于64k，但是如果这一次已经把大于64k的可写空间都填满了，那剩下的也不管了，下一次调用的时候再来读**
5. const ssize_t n = ::readv(fd, vec, iovcnt)；调用readv 读fd上的数据
   - 参数： 
        - fd: 文件描述符
        - iov: iovec 结构体数组
        - iovcnt: iov 数组的元素个数
    - 返回值：
        - 成功时返回读取的总字节数
        - 失败时返回 -1 并设置 errno
   - readv 会按照 vec 数组中元素的顺序填充数据：首先填满 vec[0] 指向的内存区域，如果还有数据且 iovcnt 大于1，则接着填满 vec[1] 指向的区域
6. 返回写入的字节数

这种设计的好处：
- 采用readv来读取数据，这样只会调用一次系统io操作，避免频繁的内核到用户的数据拷贝
- 处理大数据块：通过结合内部缓冲区和栈上的 extrabuf，读取可能超过当前 Buffer 可写容量的数据


`🟢缓冲区写操作`

就是是将 Buffer 中当前可读的数据全部写入到指定的文件描述符 fd 中
```cpp
ssize_t Buffer::writeFd(int fd,int *saveErrno){
     size_t n = ::write(fd,peek(),readableBytes());
     if(n < 0){
        *saveErrno = errno;
    }
    return n;
}
```


<p align="right"><a href="#万字剖析muduo高性能网络库设计细节">回到顶部⬆️</a></p>

## 四、multi-Reactor事件循环模块
这个模块主要由四个类构成：
![eventloop](res/事件循环.png)

### 4.1 Channel 类
这个类用于封装一个文件描述符（如套接字、管道、定时器fd等）及其相关的事件和回调。它是事件分发的核心组件之一
**channel 类的核心职责**
1. 封装文件描述符：每个 Channel 对象都与一个唯一的文件描述符关联
2. 注册感兴趣的事件：Channel 负责告诉事件循环它关心哪些类型的事件（update到eventloop,再由eventloop提交到poller中）
3. 存储事件回调：当 Poller 检测到 fd 上发生了 Channel 感兴趣的事件时，告知eventloop，由eventloop来通知 Channel 调用预先设置好的回调函数来处理这些事件
4. 生命周期管理：将channel和eventloop绑定（weak_ptr观察eventloop对象），防止在对象销毁后仍然执行其回调。
`设置回调`
```cpp
void setReadCallback(ReadEventCallback cb){ readCallback_=std::move(cb); }
void setWriteCallback(EventCallback cb){ writeCallback_=std::move(cb); }
void setCloseCallback(EventCallback cb){ closeCallback_=std::move(cb); }
void setErrorCallback(EventCallback cb){ errorCallback_=std::move(cb); }
```
用户设置的回调最终都是绑定到channel上，并交给eventloop去做监听，回调最初的设置是在tcpserver中，在第六章讲解

`绑定`
```cpp
std::weak_ptr<void> tie_{};
void Channel::tie(const std::shared_ptr<void>&obj){
    tie_=obj;
    tied_=true;
}
```
这个 tie 机制是为了解决“悬挂回调”问题：如果 Channel 所属的对象（比如一个 TcpConnection）已经被销毁，但事件循环中仍有该 Channel 的事件待处理，直接调用回调可能会访问无效内存。tie 通过 std::weak_ptr 确保只有在宿主对象存活时才执行回调。

`注册感兴趣的事件`

```cpp

void enableReading(){events_ |= KReadEvent;   update();}
void disableReading(){events_ |= ~KReadEvent; update();}
void enableWriting(){events_ |= KWriteEvent;  update();}
void disableReading() { events_ &= ~KReadEvent; update(); }
void disableWriting() { events_ &= ~KWriteEvent; update(); }

void Channel::update(){
  loop_->updateChannel(this);
}
```
- 这些方法用于修改 Channel 感兴趣的事件集合 (events_)
- 每次修改 events_ 后，都会调用 update() 方法,update调用所属eventloop的updateChannel方法，在其中调用poller_->updateChannel(channel);

`最终的事件回调执行`

```cpp
void Channel::handleEvent(Timestamp receiveTime){
    if(tied_){
        auto ptr = tie_.lock();
        if(ptr){
            handleEventWithGuard(receiveTime);
        }
    }else{
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp recieveTime){
    //关闭写端会触发epollhup事件
    if((revents_ & EPOLLHUP)&&!(revents_ & EPOLLIN)){
        if(closeCallback_)closeCallback_();
    }
    if(revents_ & EPOLLERR){
        if(errorCallback_)errorCallback_();
    }
    if(revents_&(EPOLLIN | EPOLLPRI)){
        if(readCallback_)readCallback_(recieveTime);
    }
    if(revents_ & EPOLLOUT){
        if(writeCallback_)writeCallback_();
    }
}
```
当 Poller 通知 EventLoop某个 fd 上有事件发生时，EventLoop 会调用对应 Channel 的这个方法,执行用户设置的回调。


### 4.2 Poller
Poller 类在 Reactor 模式中充当 I/O 多路复用的抽象基类。它的主要职责是监听一组文件描述符（通过 Channel 对象间接管理），并在这些文件描述符上发生事件时通知 EventLoop。具体的 I/O 多路复用机制（如 epoll, poll, select）由其派生类实现
```cpp
class Poller : noncopyable{
public:
    using ChannelList = std::vector<Channel*>;
    Poller(EventLoop *loop);
    virtual ~Poller();
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;
    virtual void removeChannel(Channel *channel) = 0;

    bool hasChannel(Channel *channel)const;
    static Poller* newDefaultPoller(EventLoop *loop);
protected:
    //key：shckfd，value：sockfd所属的Channel类型
    using ChannelMap = std::unordered_map<int,Channel*>;
    //存储所有正在被 Poller 监听的 Channel
    ChannelMap listening_channels_;
private:
    EventLoop *ownerLoop_;
};
```
着重讲一下这个类的设计和静态工厂方法

1. 静态工厂方法

    `static Poller* newDefaultPoller(EventLoop *loop);`

    **工厂方法模式设计模式封装对象的创建过程，并允许在不修改客户端代码（这里是 EventLoop）的情况下改变被创建的对象类型。**

-  在初始化时需要一个 Poller 对象来处理 I/O 多路复用。但是，具体使用哪种 Poller（例如             EpollPoller、PollPoller 或 SelectPoller）eventloop不知道，也不会把判断的逻辑放在eventloop中，EventLoop 只需要一行代码:
   ```cpp
   poller_ = Poller::newDefaultPoller(this); // 'this' 是 EventLoop 实例
   ```
   newDefaultPoller 方法将这个“决定使用哪种 Poller 并创建它”的逻辑封装起来。EventLoop 只需要调用这个静态方法，就能得到一个合适的 Poller 实例，而无需关心创建内部具体细节。
- 并且在单独的文件中对newDefaultPoller做出具体的实现
  - 如果需要添加新的 Poller 类型或者修改选择逻辑时，只需要修改 Poller.cpp 中 newDefaultPoller 的实现，而 Poller.h 的接口可以保持不变。
  - 避免在基类的文件中由去包含子类的头文件，这不符合设计思想。


2. 多态
   - virtual ~Poller(); (虚析构函数)
  
        当通过基类指针 (Poller*) 指向一个派生类对象（比如 EpollPoller*），并在程序结束或不再需要该对象时 delete 这个基类指针，如果析构函数不是虚的，那么只会调用基类 Poller 的析构函数，而派生类 EpollPoller 的析构函数不会被调用。这会导致派生类中分配的资源（如 EpollPoller 可能创建的 epoll 文件描述符）无法被正确释放，从而造成资源泄漏。

   - 纯虚函数
        ```cpp
            virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
            virtual void updateChannel(Channel *channel) = 0;
            virtual void removeChannel(Channel *channel) = 0;
        ```
        EventLoop 可以持有一个 Poller* 指针，这个指针可以指向一个 EpollPoller 对象或一个 PollPoller 对象。当 EventLoop 调用 poller->poll(...) 时，由于 poll 是虚函数，实际执行的是指针所指向的具体派生类中的 poll 实现，实现运行时多态。


### 4.3 EpollPoller


### 4.4 Eventloop




<p align="right"><a href="#万字剖析muduo高性能网络库设计细节">回到顶部⬆️</a></p>

## 五、线程池模块

<p align="right"><a href="#万字剖析muduo高性能网络库设计细节">回到顶部⬆️</a></p>

## 六、Tcp通信模块

<p align="right"><a href="#万字剖析muduo高性能网络库设计细节">回到顶部⬆️</a></p>

## 七、模块间通信

<p align="right"><a href="#万字剖析muduo高性能网络库设计细节">回到顶部⬆️</a></p>

## 八、工作流程

<p align="right"><a href="#万字剖析muduo高性能网络库设计细节">回到顶部⬆️</a></p>

## 九、总结

<p align="right"><a href="#万字剖析muduo高性能网络库设计细节">回到顶部⬆️</a></p>

## 参考文章

<p align="right"><a href="#万字剖析muduo高性能网络库设计细节">回到顶部⬆️</a></p>

## Thread EventLoopThread EventLoopThreadPool

EventLoopThread的逻辑是
构造时将thread_(std::bind(&EventLoopThread::threadFunc,this),name)绑定，作为thread类的func_回调在调用startLoop时，调用thread_.start()，创建子线程用智能指针管理，然后子线程内部又调用func_（），也就是EventLoopThread的threadFunc()方法，在这个方法内创建了一个loop,调用外部传给的EventLoopThread的callback_，然后在这个threadFunc()回调中执行loop.loop();
两个问题
1.设计成这样复杂的回调逻辑是为什么
2.thread_(std::bind(&EventLoopThread::threadFunc,this),name)2.thread_(std::bind(&EventLoopThread::threadFunc,this),name)将类内部成员方法传给thread_，但是构造时是move,这样内成员方法不就是脱离对象实例在调用吗