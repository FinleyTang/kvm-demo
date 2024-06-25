# A tiny 16-bit guest "kernel" that infinitely prints an incremented number to the debug port

.globl _start
.code16          # 16bit 模式，让 KVM 用 "real" mode 运行
_start:          # 代码开始
  xorw %ax, %ax  # 设置 %ax = 0。对同一个寄存器做异或操作，结果为 0，所以这个操作就是重置寄存器 ax。
loop:            # 开始一个循环
  out %ax, $0x10 # 将 ax 寄存器的值输出到 0x10 I/O port
  inc %ax        # 将 ax 寄存器的值加 1
  jmp loop       # 跳到下一次循环