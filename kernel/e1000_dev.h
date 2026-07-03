//
// E1000 硬件定义：寄存器与DMA环形队列格式。
// 参考 Intel 82540EP/EM 等手册。
//

/* 寄存器（基地址偏移量，以32位字为单位，即偏移量/4） */
#define E1000_CTL      (0x00000/4)  /* 设备控制寄存器 - 读写 */
#define E1000_ICR      (0x000C0/4)  /* 中断原因读取寄存器 - 只读，读取后自动清除 */
#define E1000_IMS      (0x000D0/4)  /* 中断屏蔽设置寄存器 - 读写，置位使能中断 */
#define E1000_RCTL     (0x00100/4)  /* 接收控制寄存器 - 读写 */
#define E1000_TCTL     (0x00400/4)  /* 发送控制寄存器 - 读写 */
#define E1000_TIPG     (0x00410/4)  /* 发送帧间隙寄存器 - 读写（设置帧间最小间隔） */
#define E1000_RDBAL    (0x02800/4)  /* 接收描述符基地址低32位 - 读写 */
#define E1000_RDTR     (0x02820/4)  /* 接收延迟定时器（产生中断的延迟） - 读写 */
#define E1000_RADV     (0x0282C/4)  /* 接收绝对延迟定时器（绝对中断延迟） - 读写 */
#define E1000_RDH      (0x02810/4)  /* 接收描述符头指针（硬件当前处理位置） - 读写 */
#define E1000_RDT      (0x02818/4)  /* 接收描述符尾指针（软件写入新描述符的位置） - 读写 */
#define E1000_RDLEN    (0x02808/4)  /* 接收描述符环形队列长度（字节数） - 读写 */
#define E1000_RSRPD    (0x02C00/4)  /* 接收小包检测中断（当收到小于设定大小的包时触发） - 读写 */
#define E1000_TDBAL    (0x03800/4)  /* 发送描述符基地址低32位 - 读写 */
#define E1000_TDLEN    (0x03808/4)  /* 发送描述符环形队列长度（字节数） - 读写 */
#define E1000_TDH      (0x03810/4)  /* 发送描述符头指针（硬件已发送的位置） - 读写 */
#define E1000_TDT      (0x03818/4)  /* 发送描述符尾指针（软件待发送的新描述符） - 读写 */
#define E1000_MTA      (0x05200/4)  /* 多播地址表数组（128个64位寄存器） - 读写数组 */
#define E1000_RA       (0x05400/4)  /* 接收地址表（前两个为MAC地址） - 读写数组 */

/* 设备控制寄存器（E1000_CTL）的位定义 */
#define E1000_CTL_SLU     0x00000040    /* 设置链路为Up状态（强制启用链路） */
#define E1000_CTL_FRCSPD  0x00000800    /* 强制速度（配合速度选择位） */
#define E1000_CTL_FRCDPLX 0x00001000    /* 强制双工模式（半双工/全双工） */
#define E1000_CTL_RST     0x00400000    /* 完全复位（重置设备） */

/* 发送控制寄存器（E1000_TCTL）的位定义 */
#define E1000_TCTL_RST    0x00000001    /* 软件复位发送逻辑 */
#define E1000_TCTL_EN     0x00000002    /* 使能发送 */
#define E1000_TCTL_BCE    0x00000004    /* 忙检查使能（检测发送描述符是否忙） */
#define E1000_TCTL_PSP    0x00000008    /* 填充短包（小于最小帧长时自动补零） */
#define E1000_TCTL_CT     0x00000ff0    /* 冲突阈值（重传前退避次数） */
#define E1000_TCTL_CT_SHIFT 4           /* 冲突阈值字段的位移 */
#define E1000_TCTL_COLD   0x003ff000    /* 冲突距离（半双工下延迟时间） */
#define E1000_TCTL_COLD_SHIFT 12        /* 冲突距离字段的位移 */
#define E1000_TCTL_SWXOFF 0x00400000    /* 软件发送Xoff帧（暂停流控） */
#define E1000_TCTL_PBE    0x00800000    /* 包突发使能（允许连续发送多个包） */
#define E1000_TCTL_RTLC   0x01000000    /* 晚冲突重传（发生晚冲突时重传） */
#define E1000_TCTL_NRTU   0x02000000    /* 不下溢重传（发生发送下溢不重传） */
#define E1000_TCTL_MULR   0x10000000    /* 多请求支持（允许多个DMA请求） */

/* 接收控制寄存器（E1000_RCTL）的位定义 */
#define E1000_RCTL_RST            0x00000001    /* 软件复位接收逻辑 */
#define E1000_RCTL_EN             0x00000002    /* 使能接收 */
#define E1000_RCTL_SBP            0x00000004    /* 存储坏包（接收有CRC错误的包） */
#define E1000_RCTL_UPE            0x00000008    /* 单播混杂模式（接收所有单播包） */
#define E1000_RCTL_MPE            0x00000010    /* 多播混杂模式（接收所有多播包） */
#define E1000_RCTL_LPE            0x00000020    /* 长包使能（允许接收大于标准MTU的包） */
#define E1000_RCTL_LBM_NO         0x00000000    /* 无环回模式 */
#define E1000_RCTL_LBM_MAC        0x00000040    /* MAC内部环回模式 */
#define E1000_RCTL_LBM_SLP        0x00000080    /* 串行链路环回模式 */
#define E1000_RCTL_LBM_TCVR       0x000000C0    /* 收发器环回模式 */
#define E1000_RCTL_DTYP_MASK      0x00000C00    /* 描述符类型掩码 */
#define E1000_RCTL_DTYP_PS        0x00000400    /* 包分割描述符类型 */
#define E1000_RCTL_RDMTS_HALF     0x00000000    /* 接收描述符最小阈值：一半 */
#define E1000_RCTL_RDMTS_QUAT     0x00000100    /* 接收描述符最小阈值：四分之一 */
#define E1000_RCTL_RDMTS_EIGTH    0x00000200    /* 接收描述符最小阈值：八分之一 */
#define E1000_RCTL_MO_SHIFT       12            /* 多播偏移量字段的位移 */
#define E1000_RCTL_MO_0           0x00000000    /* 多播偏移位：使用哈希表位[11:0] */
#define E1000_RCTL_MO_1           0x00001000    /* 多播偏移位：使用哈希表位[12:1] */
#define E1000_RCTL_MO_2           0x00002000    /* 多播偏移位：使用哈希表位[13:2] */
#define E1000_RCTL_MO_3           0x00003000    /* 多播偏移位：使用哈希表位[15:4] */
#define E1000_RCTL_MDR            0x00004000    /* 多播描述符环0（保留） */
#define E1000_RCTL_BAM            0x00008000    /* 广播使能（接收广播包） */
/* 当 E1000_RCTL_BSEX = 0 时，以下缓冲大小有效 */
#define E1000_RCTL_SZ_2048        0x00000000    /* 接收缓冲大小 2048 字节 */
#define E1000_RCTL_SZ_1024        0x00010000    /* 接收缓冲大小 1024 字节 */
#define E1000_RCTL_SZ_512         0x00020000    /* 接收缓冲大小 512 字节 */
#define E1000_RCTL_SZ_256         0x00030000    /* 接收缓冲大小 256 字节 */
/* 当 E1000_RCTL_BSEX = 1 时，以下缓冲大小有效 */
#define E1000_RCTL_SZ_16384       0x00010000    /* 接收缓冲大小 16384 字节 */
#define E1000_RCTL_SZ_8192        0x00020000    /* 接收缓冲大小 8192 字节 */
#define E1000_RCTL_SZ_4096        0x00030000    /* 接收缓冲大小 4096 字节 */
#define E1000_RCTL_VFE            0x00040000    /* VLAN过滤使能 */
#define E1000_RCTL_CFIEN          0x00080000    /* 规范格式指示器使能（VLAN CFI） */
#define E1000_RCTL_CFI            0x00100000    /* 规范格式指示器值 */
#define E1000_RCTL_DPF            0x00400000    /* 丢弃暂停帧（不处理流控帧） */
#define E1000_RCTL_PMCF           0x00800000    /* 传递MAC控制帧（接收所有控制帧） */
#define E1000_RCTL_BSEX           0x02000000    /* 缓冲大小扩展（选择大缓冲） */
#define E1000_RCTL_SECRC          0x04000000    /* 剥离以太网CRC（从包末尾移除CRC） */
#define E1000_RCTL_FLXBUF_MASK    0x78000000    /* 灵活缓冲大小掩码 */
#define E1000_RCTL_FLXBUF_SHIFT   27            /* 灵活缓冲大小字段的位移 */

#define DATA_MAX 1518              /* 最大数据包长度（不含CRC） */

/* 发送描述符命令字段（cmd）的位定义 [E1000 3.3.3.1] */
#define E1000_TXD_CMD_EOP    0x01 /* 包结束标志（该描述符是包的最后一个） */
#define E1000_TXD_CMD_RS     0x08 /* 报告状态标志（硬件更新状态字段） */

/* 发送描述符状态字段（status）的位定义 [E1000 3.3.3.2] */
#define E1000_TXD_STAT_DD    0x00000001 /* 描述符完成（硬件已处理该描述符） */

// [E1000 3.3.3] 发送描述符结构（共16字节）
struct tx_desc
{
  uint64 addr;      /* 数据缓冲区的物理地址（DMA地址） */
  uint16 length;    /* 待发送数据的字节长度 */
  uint8 cso;        /* 校验和偏移（TCP/UDP校验和起始偏移） */
  uint8 cmd;        /* 命令字段（EOP、RS等标志） */
  uint8 status;     /* 状态字段（由硬件更新，如DD） */
  uint8 css;        /* 校验和起始偏移（用于计算校验和的偏移） */
  uint16 special;   /* 特殊字段（如VLAN标签、分段信息） */
};

/* 接收描述符状态字段（status）的位定义 [E1000 3.2.3.1] */
#define E1000_RXD_STAT_DD       0x01    /* 描述符完成（硬件已填充数据） */
#define E1000_RXD_STAT_EOP      0x02    /* 包结束（该描述符包含包的最后一帧） */

// [E1000 3.2.3] 接收描述符结构（共16字节）
struct rx_desc
{
  uint64 addr;       /* 数据缓冲区的物理地址（DMA地址），用于存放接收数据 */
  uint16 length;     /* 实际接收数据长度（由硬件写入） */
  uint16 csum;       /* 校验和计算值（硬件计算） */
  uint8 status;      /* 状态字段（DD、EOP等） */
  uint8 errors;      /* 错误字段（CRC错误、对齐错误等） */
  uint16 special;    /* 特殊字段（如VLAN标签） */
};
