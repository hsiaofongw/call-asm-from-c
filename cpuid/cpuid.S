# 函数签名：
# void cpuid0(int *eax, char *cpu_vendor);


.global cpuid0

.text

cpuid0:
pushq %rbp              # 保存栈帧地址
movq %rsp, %rbp         # 设定新的栈帧
subq $0x10, %rsp        # 分配 0x40（十进制 64）个字节的栈空间

movq %rdi, -0x8(%rbp)
movq %rsi, -0x10(%rbp)

xorq %rax, %rax
movq -0x8(%rbp), %rsi
movq (%rsi), %rax
cpuid
movq -0x8(%rbp), %rdi
movq %rax, (%rdi)

movq -0x10(%rbp), %rdi
movl %ebx, (%rdi)
movl %edx, 0x4(%rdi)
movl %ecx, 0x8(%rdi)

addq $0x10, %rsp        # 释放栈空间。
popq %rbp               # 恢复栈帧地址。
retq                    # 返回到父函数调用这个函数的下一句（也就是 caller）。


