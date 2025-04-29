# XuanTie - E906

## 一 简介

### 1. 内核

[E906](https://www.xrvm.cn/community/download?id=4222755171580579840) 是一款基于 RISC-V 指令集的高能效嵌入式处理器，是玄铁 RISC-V MCU 产品线中的最高性能
处理器。 E906 的设计目标是，使用最低的面积和功耗成本，取得相对较高的性能指标。 E906 主要面向语
音、高端 MCU、轻量级 AI、导航、 WiFi 等应用领域。  

### 2.特点

E906 处理器体系结构的主要特点如下：
• 32 位 RISC 处理器；
• 支持 RISC-V RV32IMA[F][D]C[P] 指令集；
• 支持 RISC-V 32/16 位混编指令集；
• 32 个 32 位通用寄存器；
• 整型 5 级/浮点 7 级，单发射，顺序执行流水线；
• 可选配 BHT 和 BTB；
• 支持 RISC-V 机器模式和用户模式；
• 双周期硬件乘法器，基 4 硬件除法器；
• 可选配指令 cache，两路组相连结构， 2KiB-32KiB 可配置；
• 可选配数据 cache，两路组相连结构， 2KiB-32KiB 可配置；
• 兼容 RISC-V CLIC 中断标准，支持中断嵌套，外部中断源数量最高可配置 240 个；
• 兼容 RISC-V PMP 内存保护标准， 0/4/8/12/16 区域可配置；
• 支持 AHB-Lite 总线协议，支持三条总线：指令总线，数据总线和系统总线；
• 支持可配的性能监测单元；
• 支持玄铁扩展增强指令集  

## 二 工具

- 编译器： https://www.xrvm.cn/community/download?id=4433353576298909696
- 模拟器： https://www.xrvm.cn/community/download?id=4397435198627713024

注：若上述链接中的编译器与模拟器不能使用，可以使用下述CDK中的编译器与模拟器

- SDK：https://www.xrvm.cn/community/download?id=4397799570420076544

## 三 使用指南

搭建完成RT-Thread开发环境，在BSP根目录使用env工具在当前目录打开env。

![](figures/1.env.png)

使用前执行一次**menuconfig**命令，更新rtconfig.h配置，然后在当前目录执行**scons -j12**命令编译生成可可执行文件。

<img src="figures/2.scons.png" alt="env" style="zoom: 95%;" />

生成可执行文件，可以直接在命令行启动qemu或者配置vscode脚本借助vscode强大的插件进行图形化调试，qemu的相关命令可以查看玄铁qemu的[用户手册](https://www.xrvm.cn/community/download?id=4397435198627713024)，下述是启动qemu的命令，在powershell或命令行可直接执行下述命令，注意qemu需要导出至环境变量或者使用绝对路径。

```shell
qemu-system-riscv64 -machine smartl -nographic -kernel rtthread.elf -cpu e906
```

下述是使用vscode调试的展示。

<img src="figures/3.vscode.png" alt="env" style="zoom: 63%;" />

一起为RISC-V加油吧！