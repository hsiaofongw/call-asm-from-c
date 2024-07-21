# 玩具代码库

- [event_loop/fdset_demo.c]()：演示如何通过 select() API 实现基于 IO 复用的 echo。
- [event_loop/io_echo.c]()：演示如何通过 libevent 函数库实现基于 IO 复用的 echo。
- [event_loop/socket_mux.c]()：演示如何把来自多个 socket 的 packets 全部 mux 到 stdout，通过 select() API 实现。
- [cpuid/](cpuid/)：演示如何调用 cpuid 函数、如何暴露 C 调用接口。
- [helloworld/](helloworld/)：演示如何在 x86-64 汇编中调用 write syscall 以及如何暴露 C 调用接口。
- [read/](read/)：一个简单的 echo 程序。
