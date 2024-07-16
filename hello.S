# 一个标准输出打印函数的实现，基于 Linux syscall。
#
# 函数签名：
# void hello(char *buf, int len)
# 
# 作用：
# 向标准输出（fd=0）写入 [buf, buf+len) 内存区域的数据。


.global hello

.text

hello:
pushq %rbp              # 保存栈帧地址
movq %rsp, %rbp         # 设定新的栈帧
subq $0x40, %rsp        # 分配 0x40（十进制 64）个字节的栈空间
movq %rdi, -0x10(%rbp)  # 保存 RDI 的值到栈上，因为一会儿之后 RDI 要用来做 syscall 的参数。
movl %esi, -0x18(%rbp)  # 保存 ESI 的值到栈上，因为一会儿之后 ESI 要用来做 syscall 的参数。
movq %rax, -0x20(%rbp)  # 因为，按照设定，hello 返回 void，所以保护 RAX。
movq %rdx, -0x28(%rbp)  # 保护 RDX，因为可能 caller 不希望 RDX 无缘无故被修改。

movq $1, %rax           # RAX 存放 syscall 编号，立即数 1 表示 write 系统调用。
movq $1, %rdi           # RDI 存放 file descriptor，立即数 1 表示 stdout（标准输出）。BTW：0 表示 stdin，2 表示 stderr。
movq -0x10(%rbp), %rsi  # RSI 存放 write 的数据源的 buffer 的地址，也就是说要从哪里拿数据 write 到文件上。
xorq %rdx, %rdx         # 清空 RDX 的值，因为我们要往它的 low half 写数据。
movl -0x18(%rbp), %edx  # RDX 存放 write 的数据的长度，因为我们手头上只有 4 字节大小的数据描述长度，所以写 EDX 而不是 RDX。
syscall                 # 执行系统调用。

movq -0x20(%rbp), %rax  # 恢复被覆盖的 RAX 的值，因为按照设定 hello 返回 void。
movq -0x28(%rbp), %rdx  # 恢复被污染的 RDX 的值，因为 caller 可能不希望 RDX 被修改。
addq $0x40, %rsp        # 释放栈空间。
popq %rbp               # 恢复栈帧地址。
retq                    # 返回到父函数调用这个函数的下一句（也就是 caller）。


# 参考资料：
# 1. GNU Assembler 汇编语法参考 https://cs.lmu.edu/~ray/notes/gasexamples/
# 2. x86 汇编教程 https://www.cs.princeton.edu/courses/archive/fall20/cos318/precepts/x86_assembly_tutorial.pdf
# 3. GNU Assembler 宏指令介绍 https://ftp.gnu.org/old-gnu/Manuals/gas-2.9.1/html_chapter/as_7.html
# 4. Linux syscall 表 https://blog.rchapman.org/posts/Linux_System_Call_Table_for_x86_64/
