// 用于内核上下文切换的保存寄存器。
struct context {
  uint64 ra; // 返回地址寄存器
  uint64 sp; // 栈顶指针寄存器

  // 被调用者保存的寄存器
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// 每个 CPU 的状态。
struct cpu {
  struct proc *proc;          // 在此 CPU 上运行的进程，如果为空则表示没有进程。
  struct context context;     // 在这里调用 swtch() 进入调度器。
  int noff;                   // push_off()中的“中断禁用” 的嵌套深度。
  int intena;                 // push_off() 之前是否启用中断？
};

extern struct cpu cpus[NCPU];

// 用于 trampoline.S 中陷阱处理代码的每个进程数据。
// 单独占用一页，位于 trampoline 页的下方，不在内核页表中特殊映射。
// sscratch 寄存器指向这里。
// trampoline.S 中的 uservec 保存陷阱帧中的用户寄存器，
// 然后从陷阱帧的 kernel_sp、kernel_hartid、kernel_satp 初始化寄存器，
// 并跳转到 kernel_trap。
// trampoline.S 中的 usertrapret() 和 userret() 设置陷阱帧的 kernel_*，
// 从陷阱帧中恢复用户寄存器，切换到用户页表，并进入用户空间。
// 陷阱帧包括 callee-saved 的用户寄存器，如 s0-s11，
// 因为通过 usertrapret() 返回用户空间的路径不会通过整个内核调用栈。

/*  struct trapframe 是一个结构体，用于保存用户态程序在发生陷阱（trap）时的寄存器状态和其他相关信息。
    陷阱可以是系统调用、中断或异常。当 CPU 从用户态切换到内核态时，
    操作系统会将用户态的寄存器状态保存到 struct trapframe 中，
    以便在处理完陷阱后能够正确恢复用户态程序的执行。

*/
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // 内核页表
  /*   8 */ uint64 kernel_sp;     // 进程的内核栈顶
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // 保存的用户程序计数器
  /*  32 */ uint64 kernel_hartid; // 保存的内核 tp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0; // 通常用来存储函数的第一个返回值，也就是终端输入的指令传给a7，然后a7检测完成后，合法指令放在a0
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

enum procstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// 每个进程的状态
struct proc {
  struct spinlock lock;

  // 使用这些时必须持有 p->lock：
  enum procstate state;        // 进程状态
  struct proc *parent;         // 父进程
  void *chan;                  // 如果非零，则在 chan 上睡眠
  int killed;                  // 如果非零，则已被杀死
  int xstate;                  // 返回给父进程的退出状态
  int pid;                     // 进程 ID

  // 这些是进程私有的，因此不需要持有 p->lock。
  uint64 kstack;               // 内核栈的虚拟地址
  uint64 sz;                   // 进程内存的大小（字节）
  pagetable_t pagetable;       // 用户页表
  struct trapframe *trapframe; // trampoline.S 的数据页
  struct context context;      // 在这里调用 swtch() 运行进程
  struct file *ofile[NOFILE];  // 打开的文件
  struct inode *cwd;           // 当前目录
  char name[16];               // 进程名称（用于调试）
  int syscall_trace;        // 存储进程的系统调用的跟踪掩码，用于记录哪些系统调用需要被跟踪
};
