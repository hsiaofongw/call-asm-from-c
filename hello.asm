.global hello

.text

hello:
pushq %rbp
movq %rsp, %rbp
subq $0x20, %rsp
movq %rdi, -8(%rbp)
movl %esi, -0x10(%rbp)
movq %rax, -0x18(%rbp)

movq $1, %rax
movq $1, %rdi
movq -8(%rbp), %rsi
xorq %rdx, %rdx
movl -0x10(%rbp), %edx
syscall

movq -0x18(%rbp), %rax
addq $0x20, %rsp
popq %rbp
retq

