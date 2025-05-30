// 物理内存布局

// qemu -machine virt 的设置如下，
// 基于 qemu 的 hw/riscv/virt.c:
//
// 00001000 -- 启动 ROM，由 qemu 提供
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0 
// 10001000 -- virtio 磁盘 
// 80000000 -- 启动 ROM 在机器模式下跳转到这里
//             -kernel 将内核加载到这里
// 80000000 之后是未使用的 RAM。

// 内核使用物理内存如下:
// 80000000 -- entry.S，然后是内核文本和数据
// end -- 内核页分配区域的开始
// PHYSTOP -- 内核使用的 RAM 结束

// qemu 将 UART 寄存器放在这里的物理内存中。
#define UART0 0x10000000L
#define UART0_IRQ 10

// virtio mmio 接口
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

// 本地中断控制器，包含计时器。
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // 自启动以来的周期数。

// qemu 将可编程中断控制器放在这里。
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// 内核期望有 RAM
// 供内核和用户页使用
// 从物理地址 0x80000000 到 PHYSTOP。
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024) //128M

// 将 trampoline 页映射到最高地址，
// 在用户空间和内核空间中都是如此。
#define TRAMPOLINE (MAXVA - PGSIZE) // trampoline 页通常用于用户态和内核态之间的上下文切换。它是一个特殊的页，映射到内核的某些关键代码（例如中断处理或系统调用入口）。

// 将内核栈映射到 trampoline 下方，
// 每个周围都有无效的保护页。
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE) // 为每个进程分配内核栈

// 用户内存布局。
// 首先是地址零:
//   文本
//   原始数据和 bss
//   固定大小的栈
//   可扩展的堆
//   ...
//   TRAPFRAME (p->trapframe，由 trampoline 使用)
//   TRAMPOLINE (与内核中相同的页面)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
