# qemu-virt64-riscv SMP 修复记录

## 现象

这个 BSP 默认按单核配置运行。打开 SMP 并使用 QEMU 双核启动后，系统虽然可以进入 shell，但 secondary hart 的启动、IPI 调度和页表更新都存在问题：

- secondary CPU 启动得太晚，可能在调度器尚未准备好时收到调度 IPI。
- 调度 IPI 的向量值和 pending 位图含义不一致，调度请求可能被丢弃或执行成其他 IPI。
- IPI pending 数组是 `uint8_t`，却通过 `rt_atomic_t` 做原子操作。在 RV64 上会产生未对齐的 64 位 AMO，并可能访问数组边界之外的数据。
- `rt_hw_ipi_send()` 只处理 CPU mask 中最低的一位，不能正确广播到多个 CPU。
- qemu BSP 没有安装调度、停止和 SMP call 的 IPI handler。
- 修改页表后只刷新当前 CPU 的 TLB，其他 CPU 可能继续使用旧的页表项。

## 修改内容

### 1. 启用双核配置

将以下配置设为双核：

- `.config` 和 `rtconfig.h`：启用 `RT_USING_SMP`，设置 `RT_CPUS_NR=2`。
- `link_cpus.lds`：设置 `RT_CPUS_NR = 2`，使链接脚本和内核配置保持一致。

### 2. 调整 secondary CPU 启动时序

在 `rtthread_startup()` 中先持有 `_cpus_lock`，再调用 `rt_hw_secondary_cpu_up()`，之后创建系统线程并启动调度器。

secondary CPU 进入 `secondary_cpu_entry()` 后初始化 timer、IPI，并等待 `_cpus_lock`。主 CPU 启动调度器时释放该锁，两个 CPU 才开始正常调度。这样可以避免线程创建过程中向尚未进入调度器的 CPU 发送调度 IPI。

相关文件：

- `src/components.c`
- `libcpu/risc-v/common64/cpuport.c`

### 3. 修复 IPI 位图和并发访问

`ipi_vectors` 改为每个 CPU 一个 `rt_atomic_t`，IPI vector 使用位编码：

```c
1U << ipi_vector
```

发送时遍历 CPU mask，为每个目标 CPU 设置对应 pending 位，然后通过 SBI 发送软件中断。

处理时使用原子交换一次取出 pending 位图，再逐个调用 handler；处理期间新到达的 IPI 会在下一轮重新检查，不会覆盖或丢失旧请求。

同时增加了 vector 范围检查，并使用 RV64 对应的 `__builtin_ctzl()` 查找位号。

相关文件：

- `libcpu/risc-v/virt64/interrupt.c`

### 4. 只向已上线 CPU 发送 IPI 和 TLB 请求

增加 `rt_riscv_online_mask`：

- 主 CPU 在内核启动阶段标记为在线。
- secondary CPU 完成 IPI 初始化后标记为在线。
- IPI 发送和远端 TLB 刷新只使用在线 CPU mask。

这样可以避免 secondary CPU 还未完成初始化时收到请求。

相关文件：

- `libcpu/risc-v/common64/cpuport.c`
- `libcpu/risc-v/common64/cpuport.h`
- `libcpu/risc-v/virt64/interrupt.c`

### 5. 增加远端 TLB shootdown

在 `rt_hw_tlb_invalidate_aspace()` 和 `rt_hw_tlb_invalidate_page()` 中，通过 SBI `remote_sfence.vma` 通知在线 CPU 刷新 TLB，避免不同 CPU 使用不一致的页表缓存。

相关文件：

- `libcpu/risc-v/common64/tlb.h`

### 6. 补齐 BSP 的 IPI handler

在 `rt_hw_board_init()` 中完成：

- `rt_smp_call_init()` 初始化；
- `RT_SCHEDULE_IPI` 注册到 `rt_scheduler_ipi_handler`；
- `RT_STOP_IPI` 注册到 `rt_scheduler_ipi_handler`；
- `RT_SMP_CALL_IPI` 注册到 `rt_smp_call_ipi_handler`。

相关文件：

- `bsp/qemu-virt64-riscv/driver/board.c`

## SMP 测试命令

`applications/test/smp_test.c` 提供了可重复执行的 shell 命令：

```text
msh /> smp_test 100
cpu0: worker=100/100, smp_call=100/100
cpu1: worker=100/100, smp_call=100/100
smp_test: PASS (errors=0)
```

测试同时检查两项内容：绑定到 CPU0/CPU1 的线程是否始终运行在目标 CPU，以及同步 SMP-call 是否在每个 CPU 上执行指定次数。
