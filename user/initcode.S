# Initial process that execs /init.
# This code runs in user space.
#
# 第一个进程中, fork 的参数会被设置为调用 /init, 走到这里.

#include "syscall.h"

# exec(init, argv). 调用系统调用 exec, 自动切到 init 进程.
.globl start
start:
        la a0, init
        la a1, argv
        li a7, SYS_exec
        ecall

# for(;;) exit();
exit:
        li a7, SYS_exit
        ecall
        jal exit

# char init[] = "/init\0";
init:
  .string "/init\0"

# char *argv[] = { init, 0 };
.p2align 2
argv:
  .long init
  .long 0
