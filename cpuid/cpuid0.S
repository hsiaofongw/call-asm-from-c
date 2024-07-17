# 函数签名：
# void cpuid0(int *eax, char *cpu_vendor);
# 作用
# 查看 CPU Vendor 名称。

.global cpuid0

.text

cpuid0:
pushq %rbp              # 保存栈帧地址
movq %rsp, %rbp         # 设定新的栈帧
subq $0x10, %rsp        # 因为没有那么多通用寄存器可以辗转腾挪，所以分配栈空间用来保存寄存器的值，

movq %rdi, -0x8(%rbp)   # 把 RDI 也就是第一个实参（#1）入栈保存
movq %rsi, -0x10(%rbp)  # 把 RSI 也就是第二个实参（#2）入栈保存

xorq %rax, %rax         # 清空 RAX，包括它的高 32 位和低 32 位。
movq -0x8(%rbp), %rsi   # 加载之前保存的第一个实参，注意 #1 其实是指针，那么也就是说现在 RSI 寄存器存的值是一个内存地址。
movq (%rsi), %rax       # 对这个指针提领，相当于 RAX = *(#1)，也就是 RSI 存放的地址处拿一个 Quad（8位数据），然后存到 RAX 里边。
cpuid                   # 执行 CPUID 指令。读写 EAX，写入 EAX、EBX、EDX 和 ECX。
movq -0x8(%rbp), %rdi   # 把内存地址加载到 RDI，令 RDI「成为」一个指针。
movq %rax, (%rdi)       # 把位于 RAX 的 CPUID 返回值写到 *RDI，注意到这里的 RDI 其实就是函数第一个参数（是一个指针）。通过这种方式我们把这里 RAX 的值传给 caller。

movq -0x10(%rbp), %rdi  # 把内存地址加载到 RDI，令 RDI 成为一个指针，它实际上就是 char *cpu_vendor，也就是第二个形参。
movl %ebx, (%rdi)       # 写入 CPU vendor 前 4 个字符。相当于写入到 cpu_vendor[0:4]
movl %edx, 0x4(%rdi)    # 写入 CPU vendor 中间 4 个字符。相当于写入到 cpu_vendor[4:8]
movl %ecx, 0x8(%rdi)    # 写入 CPU vendor 最后 4 个字符。相当于写入到 cpu_vendor[8:12]

addq $0x10, %rsp        # 释放栈空间。
popq %rbp               # 恢复栈帧地址。
retq                    # 返回到父函数调用这个函数的下一句（也就是 caller）。
