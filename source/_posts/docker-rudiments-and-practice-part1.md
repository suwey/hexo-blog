---
title: docker入门实战笔记
tags: [docker, Linux]
categories: docker
---

这篇文章主要是[Docker入门实战](http://yuedu.baidu.com/ebook/d817967416fc700abb68fca1?fr=aladdin&key=docker%E5%85%A5%E9%97%A8%E5%AE%9E%E6%88%98)这本电子书的学习记录，对其中一些原理相关代码的记录和实践就不再赘述一些更为基本的概念，这本书是由[DockerOne](http://www.dockone.io)编译的，其条理和内容都很不错非常值得一读。继续阅读之前需要先了解下容器是什么、和VM的区别是什么、Docker解决了什么问题以及镜像、数据卷、LXC、Cgroups、Union文件系统等基本概念，然后安装好docker和gcc就可以愉快的进入下文了，如果对Docker的源码有兴趣可以看看这个系列的文章[列表](http://www.infoq.com/cn/search.action?queryString=Docker%E6%BA%90%E7%A0%81%E5%88%86%E6%9E%90&page=1&searchOrder=&sst=uZnwt8z7LUYMOcuH)。

<!-- more -->

## LXC 核心：Linux Namespaces
Docker是利用LXC实现类似VM的功能的（之后换成了libcontainer，具体区别和对libcontainer的分析请看[这里](http://www.infoq.com/cn/articles/docker-container-management-libcontainer-depth-analysis)），而LXC依赖Linux内核的3种隔离机制：

1. Chroot
2. Cgroups
3. Namespaces

本文主要关注Namespaces，在3.12 kernel中Linux支持以下6种Namespace：

1. UTS: hostname
2. IPC: 进程间通信
3. PID: Chroot 进程树
4. NS: 挂载点
5. NET: 网络访问，包括接口
6. USER: user-id映射

## UTS Namespace

关于上述内容在[这里](http://www.infoq.com/cn/articles/docker-core-technology-preview)有更详细的解释，下面试着在容器中启动`/bin/bash`，编写代码[main-0-template.c](/attachments/docker/practice/main-0-template.c)：

```c
// Last Update:2016-02-25 09:15:39
/**
 * @file main-0-template.c
 * @brief docker lxc test
 * @author suwey_1990@126.com
 * @version 0.0.1
 * @date 2016-02-25
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>
#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];
char* const child_args[] = {
    "/bin/bash",
    NULL
};
int child_main(void* arg) {
    printf(" - World !\n");
    execv(child_args[0], child_args);
    printf("Ooops\n");
    return 1;
}
int main() {
    printf(" - Hello ?\n");
    int child_pid = clone(child_main, child_stack + STACK_SIZE, SIGCHLD, NULL);
    waitpid(child_pid, NULL, 0);
    return 0;
}
```

编译执行`gcc -Wall main-0-template.c -o ns && sudo ./ns`可以看到输出，从结果看就是很普通的helloworld只不过是在子进程中输出的，然后试着修改下主机名，修改代码为[main-1-uts.c](/attachments/docker/practice/main-1-uts.c)：

```c
//require root
int child_main(void* arg) {
    printf(" - World !\n");
    sethostname("In Namespace", 12);
    execv(child_args[0], child_args);
    printf("Ooops\n");
    return 1;
}
int main() {
    printf(" - Hello ?\n");
    int child_pid = clone(child_main, child_stack + STACK_SIZE, CLONE_NEWUTS | SIGCHLD, NULL);
    waitpid(child_pid, NULL, 0);
    return 0;
}
```

主要不同是使用了`CLONE_NEWUTS`标记，运行后会发现hostname已经被修改了如果没有调用`exit`命令则会一直处在容器中，可以看到使用Namespace技术非常简单，只需要调用`clone`函数并设置好标记，下面继续学习下IPC是如何隔离的。


## IPC Namespace
很自然的可以想到肯定有一个别的标记对应此类Namespace，这个标记就是`CLONE_NEWIPC`，但是这次有点不同的是这里有父子进程通信的问题需要解决，clone和它的parent会分享内存空间，因此有以下几种方式可以使用：

* 信号
* poll内存
* 套接字
* 文件和文件描述符

这几种里面首先信号可以直接排除，因为在容器内的进程上下文已经和父进程不一样了，至于poll内存的方式因为效率问题暂不考虑，socket套接字这种方法和文件的方式都可以但是这里提供一种由`Lennart Poettering`在`Systemd's "nspawn"`中使用过的技术，通过监视一对`pipe`上的事件来完成通信。首先需要初始化一对pipe：

```c
#include <unistd.h>
int checkpoint[2];
pipe(checkpoint);
```

这里的思想是在parent中触发一个`close`事件，并且在child的读取端等待`EOF`被接收，接收后所有写fd的文件必须被关闭，所以在child等待前就要关闭fd的副本：

```c
close(checkpoint[1]);
```

基于上文模板需要修改部分代码[main-2-ipc.c](/attachments/docker/practice/main-2-ipc.c)：

```c
int child_main(void* arg) {
    char c;
    close(checkpoint[1]);
    read(checkpoint[0], &c, 1);

    printf(" - World !\n");
    sethostname("In Namespace", 12);
    execv(child_args[0], child_args);
    printf("Ooops\n");
    return 1;
}
int main() {
    pipe(checkpoint);

    printf(" - Hello ?\n");

    int child_pid = clone(child_main, child_stack + STACK_SIZE, CLONE_NEWIPC | CLONE_NEWUTS | SIGCHLD, NULL);
    sleep(4);
    close(checkpoint[1]);

    waitpid(child_pid, NULL, 0);
    return 0;
}
```

执行此程序可以观察到在父进程完成sleep后容器中的子进程才获得消息然后输出内容，这里还保留着UTS的标志只是为了顺便说明Namespace是可以组合使用的，本质上也是通过各种组合实现不同程度的隔离。


## PID Namespace
很自然的可以猜到这次的标记是`CLONE_NEWPID`，激活此Namespace后将会重置PID计数，子进程`getpid()`的返回结果将会是1，使用[main-3-pid.c](/attachments/docker/practice/main-3-pid.c)进行测试：

```c
int child_main(void* arg) {
    char c;
    close(checkpoint[1]);
    read(checkpoint[0], &c, 1);
    printf(" - [%5d] World !\n", getpid());
    sethostname("In Namespace", 12);
    execv(child_args[0], child_args);
    printf("Ooops\n");
    return 1;
}
int main() {
    pipe(checkpoint);
    printf(" - [%5d] Hello ?\n", getpid());
    int child_pid = clone(child_main, child_stack + STACK_SIZE, CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS | SIGCHLD, NULL);
    close(checkpoint[1]);
    waitpid(child_pid, NULL, 0);
    return 0;
}
```

运行后会发现子进程的pid为1，在子进程中即使Kill父进程的pid也只会提示`bash: kill: (1674) - No such process`，这一点和`chroot`很像有趣的是此时如果在父进程中使用`top`将可以看到子进程和它未映射的pid，但是如果在子进程中运行则会发现和父进程是一样的内容，这是因为这类工具往往直接从`/proc`中获取信息，后面将会讨论如何让子进程也看起来比较正常。


## NS Namespace
直接开始吧，使用[main-4-ns.c](/attachments/docker/practice/main-4-ns.c)进行测试：

```c
int child_main(void* arg) {
    char c;
    close(checkpoint[1]);
    printf(" - [%5d] World !\n", getpid());
    sethostname("In Namespace", 12);
    mount("proc", "/proc", "proc", 0, NULL);
    read(checkpoint[0], &c, 1);
    execv(child_args[0], child_args);
    printf("Ooops\n");
    return 1;
}
int main() {
    pipe(checkpoint);
    printf(" - [%5d] Hello ?\n", getpid());
    int child_pid = clone(child_main, child_stack + STACK_SIZE, CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS | SIGCHLD, NULL);
    close(checkpoint[1]);
    waitpid(child_pid, NULL, 0);
    return 0;
}
```

这一次将会解决子进程查看命令在上一节中的问题，如果重新挂载基本的文件系统可以在子进程中只能看到自己的设备、文件等等，有兴趣可以自行尝试。不过这里有个问题是当退出子进程后会发现再也使用不了sudo了，提示`sudo: no tty present and no askpass program specified`，如果使用ps命令则会有错误`Error, do this: mount -t proc proc /proc`，但是mount命令又需要用sudo来执行，所以这时候如果想使用和`/proc`相关的命令只能强制重启机器，如果不是虚拟机的话得亲自去机房关机哦。其实这个错误信息已经告诉了此问题的原因，所以解决办法就是在主进程的函数中再调用一次mount，完整文件已经修改可以放心运行。


## NET Namespace
这次先不急着上代码，先走一遍基本原理，先查看下本机网络情况：

```
sudo ip link list
[sudo] password for suwey:
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN mode DEFAULT group default qlen 1
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
2: eno16777736: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP mode DEFAULT group default qlen 1000
    link/ether 00:0c:29:ee:fe:5d brd ff:ff:ff:ff:ff:ff
```

可以看到本机的lo口是启动的，然后直接使用命令创建Net Namespace看看里面的情况：

```
sudo ip netns add demo
sudo ip netns exec demo ip link list
1: lo: <LOOPBACK> mtu 65536 qdisc noop state DOWN mode DEFAULT group default qlen 1
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
```

可以看到其内部的lo口状态是DOWN，和外层的lo互不干扰与之前IPC有同样的隔离级别，如果要在外面和里面的网络进行通信的话可以创建一个P2P管道，使用内核提供的`veth`接口可以完成这件事情，如果想其可以监听外部主机的端口可以考虑使用`DNAT`技术，现在可以使用代码[main-5-net.c](/attachments/docker/practice/main-5-net.c)测试了：

```c
int child_main(void* arg) {
    char c;
    close(checkpoint[1]);
    printf(" - [%5d] World !\n", getpid());
    sethostname("In Namespace", 12);
    mount("proc", "/proc", "proc", 0, NULL);
    read(checkpoint[0], &c, 1);
    system("ip link set lo up");
    system("ip link set veth1 up");
    system("ip addr add 192.168.55.222/24 dev veth1");
    execv(child_args[0], child_args);
    printf("Ooops\n");
    return 1;
}
int main() {
    pipe(checkpoint);
    printf(" - [%5d] Hello ?\n", getpid());
    int child_pid = clone(child_main, child_stack + STACK_SIZE, CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS | SIGCHLD, NULL);
    char* cmd;
    asprintf(&cmd, "ip link set veth1 netns %d", child_pid);
    system("ip link add veth0 type veth peer name veth1");
    system(cmd);
    system("ip link set veth0 up");
    system("ip addr add 192.168.55.1/24 dev veth0");
    free(cmd);
    close(checkpoint[1]);
    waitpid(child_pid, NULL, 0);
    mount("proc", "/proc", "proc", 0, NULL);
    return 0;
}
```

然后可以看看这时候内部的情况：

```
[root@In Namespace my]# ip addr
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
    inet6 ::1/128 scope host
       valid_lft forever preferred_lft forever
3: veth1@if4: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc noqueue state UP group default qlen 1000
    link/ether 3e:59:c2:bd:59:ad brd ff:ff:ff:ff:ff:ff link-netnsid 0
    inet 192.168.55.222/24 scope global veth1
       valid_lft forever preferred_lft forever
    inet6 fe80::3c59:c2ff:febd:59ad/64 scope link
       valid_lft forever preferred_lft forever
[root@In Namespace my]# ping 192.168.55.1
PING 192.168.55.1 (192.168.55.1) 56(84) bytes of data.
64 bytes from 192.168.55.1: icmp_seq=1 ttl=64 time=0.080 ms
64 bytes from 192.168.55.1: icmp_seq=2 ttl=64 time=0.074 ms
64 bytes from 192.168.55.1: icmp_seq=3 ttl=64 time=0.120 ms
^C
--- 192.168.55.1 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2000ms
rtt min/avg/max/mdev = 0.074/0.091/0.120/0.021 ms
[root@In Namespace my]# route add default gw 192.168.55.1
[root@In Namespace my]# ping 192.168.45.138
PING 192.168.45.138 (192.168.45.138) 56(84) bytes of data.
64 bytes from 192.168.45.138: icmp_seq=1 ttl=64 time=0.072 ms
64 bytes from 192.168.45.138: icmp_seq=2 ttl=64 time=0.129 ms
64 bytes from 192.168.45.138: icmp_seq=3 ttl=64 time=0.115 ms
64 bytes from 192.168.45.138: icmp_seq=4 ttl=64 time=0.113 ms
```

可以看到添加了路由后是可以连通外部本机地址的，至此就演示完了对网络隔离的方法了，想进一步了解网络虚拟化的话可以查一下Linux kernel新支持的接口类型：macvlan、vlan、vxlans...。


## USER Namespace
这个Namespace在入门实战这个电子书中没有讲，但是像我这种有追求得读者怎么可以让这篇文章最后少了这么一个重要的内容，于是通过牛逼的搜索引擎终于还是补上了，主要是参考了[这篇](https://lwn.net/Articles/532593)。结合前面的内容显然这次的标记是`CLONE_NEWUSER`，与前面有一点不同的是如果仅使用此标记是不需要使用root权限的，先运行[demo-userns.c](/attachments/docker/practice/demo-userns.c)简单看一下效果：

```c
int child_main(void* arg) {
      cap_t caps;
      for(;;) {
          printf("eUID = %ld, eGID = %ld\n", (long)geteuid(), (long)getegid());
          caps = cap_get_proc();
          printf("capabilities: %s\n", cap_to_text(caps, NULL));
          if(arg == NULL)
              break;
          sleep(5);
      }
      return 0;
  }
int main(int argc, char* argv[]) {
      int child_pid = clone(child_main, child_stack + STACK_SIZE, CLONE_NEWUSER | SIGCHLD, argv[1]);
      waitpid(child_pid, NULL, 0);
      return 0;
}
```

注意编译前需要运行`sudo dnf install libcap-devel`，编译时需要指定`-lcap`，结果如下：

```
eUID = 65534, eGID = 65534
capabilities: = cap_chown,cap_dac_override,cap_dac_read_search,cap_fowner,cap_fsetid,cap_kill,cap_setgid,cap_setuid,cap_setpcap,cap_linux_immutable,cap_net_bind_service,cap_net_broadcast,cap_net_admin,cap_net_raw,cap_ipc_lock,cap_ipc_owner,cap_sys_module,cap_sys_rawio,cap_sys_chroot,cap_sys_ptrace,cap_sys_pacct,cap_sys_admin,cap_sys_boot,cap_sys_nice,cap_sys_resource,cap_sys_time,cap_sys_tty_config,cap_mknod,cap_lease,cap_audit_write,cap_audit_control,cap_setfcap,cap_mac_override,cap_mac_admin,cap_syslog,cap_wake_alarm,cap_block_suspend,37+ep
```

这里最后面显示的是子进程的权限，关于这一块的详情请搜索Linux内核中一种新的权限机制`Capabilities`（可以看[这里](http://www.cnblogs.com/iamfy/archive/2012/09/20/2694977.html)了解下），而上面的uid和gid则因为在没有对外部和内部的id进行映射时将分别返回`/proc/sys/kernel/overflowuid`和`/proc/sys/kernel/overflowgid`，要注意的一点是这里子进程拥有的权限仅仅针对的是内部对于外部无论调用者拥有何种权限子进程都没有任何权限。那么如何进行映射呢？通过`/proc/PID/uid_map`和`/proc/PID/gid_map`即可完成对应的映射，内容格式为`ID-inside-ns   ID-outside-ns   length`，`ID-inside-ns`和`length`一起决定Namespace内部映射到外部的id的范围而`ID-outside-ns`表示外部id范围的开始的值，但是这个值会如何解释在不同情况所有不同，主要取决于打开`/proc/PID/uid_map`（或`/proc/PID/gid_map`）的进程：

1. 如果其与目标进程在同一个Namespace中，`ID-outside-ns`会被解释为父USER Namespace中的uid（或gid）
2. 如果其与目标进程不在同一个Namespace中，`ID-outside-ns`会被解释为打开`/proc/PID/uid_map`（或`/proc/PID/gid_map`）进程所在USER Namespace中的uid（或gid）

下面先用命令来理解下这两条规定，执行刚才的程序，随便通过参数传入一个字符串，这时候命令会不停的像刚才一样打印子进程的uid和gid，然后在另一个终端窗口执行`ps -C ns -o 'pid uid comm'`可以看到：

```
PID   UID COMMAND
29104  1000 ns        #父id
29105  1000 ns        #子id
```

然后修改一下map文件`echo '0 1000 1' > /proc/29105/uid_map`，这时候在原来的终端窗口可以看到：

```
eUID = 0, eGID = 65534
capabilities: = cap_chown,cap...
```

通过修改映射将父进程中为1000的uid关联为0，所以子进程获得的uid之后就也变成了0。关于修改映射文件也是有一定规则的：

1. 写入的进程在要映射的PID所在的USER Namespace中必须拥有`CAP_SETUID`（对于gid来说是`CAP_SETGID`）权限
2. 不管`Capabilities`设置如何，写入的进程必须在要在映射PID所在的USER Namespace中或者其父USER Namespace中
3. 至少满足下面的一条
  * 映射文件中有且仅有一条数据将写入进程在父USER Namespace中的uid（或gid）映射为子USER Namespace的uid（或gid）。这个规则允许子进程自己修改映射。
  * 在父USER Namespace中拥有`CAP_SETUID`（对于gid来说是`CAP_SETGID`）权限的进程可以定义任何在父USER Namespace中进程的映射

继续完成之前的代码[main-6-user.c](/attachments/docker/practice/main-6-user.c)然后测试下：

```c
void set_id_map(long pid, long inside, long outside, long length, char* map) {
    char path[256];
    sprintf(path, "/proc/%ld/%s", pid, map);
    FILE* file = fopen(path, "w");
    fprintf(file, "%ld %ld %ld", inside, outside, length);
    fclose(file);
}
int child_main(void* arg) {
    char c;
    close(checkpoint[1]);
    printf(" - [%5d] World !\n", getpid());
    sethostname("In Namespace", 12);
    mount("proc", "/proc", "proc", 0, NULL);
    printf("eUID = %ld, eGID = %ld\n", (long)geteuid(), (long)getegid());
    read(checkpoint[0], &c, 1);
    printf("eUID = %ld, eGID = %ld\n", (long)geteuid(), (long)getegid());
    system("ip link set lo up");
    system("ip link set veth1 up");
    system("ip addr add 192.168.55.222/24 dev veth1");
    execv(child_args[0], child_args);
    printf("Ooops\n");
    return 1;
}
int main() {
    pipe(checkpoint);
    printf(" - [%5d] Hello ?\n", getpid());
    int child_pid = clone(child_main, child_stack + STACK_SIZE, CLONE_NEWUSER | CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS | SIGCHLD, NULL);
    char* cmd;
    asprintf(&cmd, "ip link set veth1 netns %d", child_pid);
    system("ip link add veth0 type veth peer name veth1");
    system(cmd);
    system("ip link set veth0 up");
    system("ip addr add 192.168.55.1/24 dev veth0");
    free(cmd);
    set_id_map((long)child_pid, 0, (long)geteuid(), 1, "uid_map");
    set_id_map((long)child_pid, 0, (long)getegid(), 1, "gid_map");
    close(checkpoint[1]);
    waitpid(child_pid, NULL, 0);
    mount("proc", "/proc", "proc", 0, NULL);
    return 0;
}
```

通过在父进程中修改映射可以看到子进程前后两次获得的uid和gid均不同，其实根据刚才讲到的规则子进程也可以将自己的映射的修改掉，只需要对此代码稍作改动即可有兴趣可以自行测试，需要注意的一点是这里因为加上了其他标记所以必须使用root运行那么子进程修改的时候不能设置outside为1000，会提示`RTNETLINK answers: Operation not permitted`，因为这时候使用的root用户其用户组为0，关于此节内容想深入理解还可以看看这个[bug分析](http://www.seteuid0.com/cve-2013-1959%E5%86%85%E6%A0%B8%E6%BC%8F%E6%B4%9E%E5%8E%9F%E7%90%86%E4%B8%8E%E6%9C%AC%E5%9C%B0%E6%8F%90%E6%9D%83%E5%88%A9%E7%94%A8%E4%BB%A3%E7%A0%81%E5%AE%9E%E7%8E%B0%E5%88%86%E6%9E%90/)。
