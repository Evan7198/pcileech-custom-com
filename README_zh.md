[English](README.md) | [中文](README_zh.md)

<a href="https://lattecode.cn" target="_blank">
  <img src="https://img.shields.io/badge/Sponsor-LatteCode-FFDD00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black" alt="LatteCode Sponsor">
</a>

# PCILeech 自定义寄存器通信系统

本项目是 PCILeech FPGA 自定义寄存器通信系统的实现，专注于核心的 DNA 安全验证与寄存器通信功能。系统基于 Xilinx Artix-7 FPGA 芯片，通过 USB 3.0 (FT601) 实现高速通信，支持基于 57 位硬件 DNA 的身份验证机制。

## 核心特性

- **32 个寄存器架构**：24 个通用自定义寄存器（0-23）+ 8 个 DNA 验证寄存器（24-31）
- **FPGA DNA 验证**：基于 57 位硬件唯一标识的身份认证
- **TLP 访问控制**：基于 DNA 验证的寄存器访问控制
- **USB 3.0 高速**：FT601 芯片，最高支持 400MB/s 传输速率
- **自定义 LeechCore API**：扩展 API，无缝实现寄存器读写操作

## 快速开始

```bash
# 1.替换文件
替换pcileech_fifo.sv文件

# 2. 编译
编译软件和硬件项目

# 3. 硬件连接
通过 USB 3.0 连接 PCILeech FPGA 设备

# 4. DNA 激活（必须先执行！）
cd demo/ACTIVATOR_DEMO/x64/Release
activator.exe

# 5. 运行寄存器演示
cd demo/CUSTOMREGS_DEMO/x64/Release
demo.exe
```

## 文档导航

- **[PCILeech 架构与通信原理](./PCILeech架构与通信原理.md)** - 系统架构与软硬件通信机制详解
- **[演示程序总览](./demo/README_zh.md)** - 两个演示程序的使用说明
- **[DNA 激活器说明](./demo/ACTIVATOR_DEMO/README_zh.md)** - DNA 验证激活器使用指南
- **[自定义寄存器示例](./demo/CUSTOMREGS_DEMO/README_zh.md)** - API 使用标准示例说明

## 联系方式

**Discord 社区**：[点击加入](https://discord.gg/PwAXYPMkkF)

## 开源许可

本项目基于 PCILeech & LeechCore 开源项目定制开发，遵循原项目的开源许可协议。

**项目版本**：v1.0
**最后更新**：2026-2-3
