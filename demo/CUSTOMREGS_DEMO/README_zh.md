[English](README.md) | [中文](README_zh.md)

# PCILeech 自定义寄存器演示程序

本目录包含 PCILeech 自定义寄存器系统的标准 API 使用示例，演示如何进行 FPGA DNA 验证激活和寄存器读写操作。

### 快速开始

#### 1. 环境要求

**硬件**:
- PCILeech FPGA 设备（Artix-7 35T/75T/100T）
- FT601 USB 3.0 接口

**软件**:
- Windows 10/11 x64
- Visual Studio 2022
- FTD3XX 驱动已安装

#### 2. 编译

1. 双击打开 `demo.sln`
2. 在 Visual Studio 2022 中选择配置（Debug 或 Release）
3. 按 `F5` 或 `Ctrl+Shift+B` 编译

#### 3. 运行

- **在 Visual Studio 中**: 按 `F5` 运行并调试
- **独立运行**: `x64/Release/demo.exe`


### API 使用指南

#### 寄存器读写 API

```c
#include "leechcore.h"

// 自定义命令定义
#define LC_CMD_FPGA_CUSTOM_READ_REG(n)  (0x0200000000000000 | ((n) & 0xFF))
#define LC_CMD_FPGA_CUSTOM_WRITE_REG(n) (0x0201000000000000 | ((n) & 0xFF))

HANDLE hLC = LcCreateEx(&cfg, NULL);

// 读取寄存器 5
PBYTE pbDataOut = NULL;
DWORD cbDataOut = 0;
if (LcCommand(hLC, LC_CMD_FPGA_CUSTOM_READ_REG(5), 0, NULL, &pbDataOut, &cbDataOut)) {
    DWORD value = *(PDWORD)pbDataOut;
    printf("寄存器 5 = 0x%08X\n", value);
    LocalFree(pbDataOut);
}

// 写入寄存器 10
DWORD value = 0xDEADBEEF;
LcCommand(hLC, LC_CMD_FPGA_CUSTOM_WRITE_REG(10), sizeof(DWORD), (PBYTE)&value, NULL, NULL);

LcClose(hLC);
```

### 寄存器映射表（32个寄存器）

| 寄存器 | 用途 | 读写属性 | 说明 |
|--------|------|----------|------|
| 0-23 | 通用自定义寄存器 | 读写 | 用户可用，需 DNA 激活后才可访问 |
| 24 | DNA 值低 32 位 | 只读 | FPGA 硬件 DNA 的低 32 位 |
| 25 | DNA 值高 25 位 | 只读 | FPGA 硬件 DNA 的高 25 位 |
| 26 | 加密随机值 | 只读 | FPGA 生成的随机数（用于验证） |
| 27 | 解密结果 | 写入 | 软件写入的解密结果 |
| 28 | 验证状态 | 只读 | DNA 验证状态 (0=失败, 1=成功) |
| 29 | TLP 控制状态 | 只读 | TLP 访问控制 (0=禁用, 1=启用) |
| 30 | 控制命令 | 写入 | 控制命令 (1=开始验证) |
| 31 | 系统状态 | 只读 | 系统状态 (0=空闲, 1=处理中, 2=完成) |

### 重要注意事项

#### 必须先执行 DNA 激活！

**原因**：
- FPGA 固件中的 TLP 访问控制在 DNA 验证成功前处于禁用状态
- 未激活时访问寄存器 0-23 将返回全 0 或失败
- 寄存器 24-31（DNA 验证区）可在激活前读取，但 TLP 功能不可用

**正确的使用顺序**：
1. ✅ 连接 FPGA 设备 (`LcCreateEx`)
2. ✅ 读取 DNA 值（寄存器 24-25）
3. ✅ 检查 TLP 状态（寄存器 29）
4. ✅ 确认 TLP 已启用
5. ✅ 进行寄存器操作（0-23）

### 故障排查

#### 问题1: "无法连接到 FPGA 设备"

**可能原因**：
- FPGA 设备未连接
- FTD3XX 驱动未安装
- DLL 文件缺失

**解决方法**：
1. 检查 FPGA 是否已通过 USB 连接
2. 在设备管理器中确认 FT601 设备存在
3. 确认 `leechcore.dll` 和 `FTD3XX.dll` 在可执行文件目录

#### 问题2: DNA 激活后仍然无法访问寄存器

**可能原因**：
- FPGA 固件未正确实现 DNA 验证逻辑
- TLP 控制未正确启用
- 寄存器 29 的值不正确

**解决方法**：
1. 读取寄存器 29，确认值为 `0x00000001`
2. 检查 FPGA 固件中的 DNA 验证状态机
3. 确认固件版本与软件匹配

---

**Project Status**: ✅ Core functionality complete | **Register Count**: 32 (0-31)
