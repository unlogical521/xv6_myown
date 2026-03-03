set pagination off
set confirm off
set disassemble-next-line on
set print pretty on

# 如果你用 gdb-multiarch，强制架构更稳
set architecture riscv:rv64

# 加载符号（必须在 target remote 之前/之后都可，但建议先 file）
file kernel/kernel

define hook-stop
  printf "\n--- STOP ---\n"
  printf "pc      = "
  p/x $pc
  printf "sp      = "
  p/x $sp
  printf "satp    = "
  p/x $satp
  printf "sstatus = "
  p/x $sstatus
  printf "SPP(bit8, sret->U=0 S=1) = "
  p (($sstatus >> 8) & 1)
  printf "inst: "
  x/i $pc
  bt 3
end

# 连接 QEMU gdbstub
target remote localhost:26000

# 关键阶段断点（util 分支一般都有这些）
# 1) 早期内核
b main

# 2) 创建第一个用户进程
b userinit

# 3) trap返回到用户态之前的准备（会设置用户页表、准备 sret）
b usertrapret

# 4) 常见调度点（可选，想看调度过程再开）
# b scheduler

# 5) 断在用户入口地址（initcode 通常从 VA=0 开始）
# b *0x0

# 继续运行
continue