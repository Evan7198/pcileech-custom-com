[English](README.md) | [中文](README_zh.md)

# PCILeech FPGA DNA 激活器 - 演示程序

⚠️本项目没有经过编译测试，如有错误，请自行修复。

### 项目简介

本项目是一个**简单的 DEMO 演示程序**，展示如何使用 LeechCore API 进行 FPGA DNA 验证和激活。

**项目类型**: DNA 验证演示程序
**编译器**: Visual Studio 2022 (MSVC v143)
**目标平台**: Windows x64
**版本**: v1.0 DEMO

### 特性

- ✅ **代码简洁**：单一 main.c 主程序 + 工具函数
- ✅ **注释详尽**：完整的中英文注释，易于理解
- ✅ **步骤清晰**：5 个步骤演示完整激活流程
- ✅ **易于扩展**：保留核心工具函数，方便二次开发

### 快速开始

#### 编译运行

```bash
# 1. 打开 Visual Studio 2022
双击 activator.sln

# 2. 选择 Release | x64 配置

# 3. 按 F7 编译

# 4. 运行
cd x64\Release
activator.exe -v
```

#### 预期输出

```
====================================================
  PCILeech FPGA DNA 激活器 v1.0 DEMO
====================================================

步骤 1: 连接 FPGA 设备 → [PASS]
步骤 2: 读取 FPGA DNA 值 → [PASS]
步骤 3: 执行 DNA 验证流程 → [PASS]
步骤 4: 检查 TLP 控制状态 → [PASS]
步骤 5: 激活摘要 → ✅ 成功

====================================================
  ✓ DNA 激活演示完成
====================================================
```

### 核心功能

#### 演示步骤

1. **连接 FPGA 设备** - 初始化 LeechCore
2. **读取 DNA 值** - 读取 57 位硬件唯一标识（寄存器 24-25）
3. **执行验证流程** - 自定义 XOR 加密验证（寄存器 26-31）
4. **检查 TLP 状态** - 验证访问控制（寄存器 29）
5. **显示激活摘要** - 完整的激活信息

#### API 使用示例

```c
// 1. 连接设备
HANDLE hLC = init_leechcore("fpga");

// 2. 读取 DNA
uint64_t dna = read_dna_value(hLC);

// 3. 启动验证
start_verification(hLC);

// 4. 读取加密值并解密
uint32_t enc = read_encrypted_value(hLC);
uint32_t dec = dna_decrypt(enc, dna);
write_decrypted_result(hLC, dec);

// 5. 检查状态
int status = check_verification_status(hLC);
int tlp = check_tlp_control_status(hLC);

// 6. 清理
cleanup_leechcore(hLC);
```

### 寄存器映射（24-31）

| 寄存器 | 功能 | 读写 | 说明 |
|--------|------|------|------|
| 24-25 | DNA 值（57位） | 只读 | FPGA 硬件 DNA |
| 26 | FPGA 加密值 | 只读 | 自动生成的加密随机数 |
| 27 | 解密结果 | 写入 | 软件写入的解密结果 |
| 28 | 验证状态 | 只读 | 0=失败, 1=成功 |
| 29 | TLP 控制 | 只读 | 0=禁用, 1=启用 |
| 30 | 控制命令 | 写入 | 1=开始验证 |
| 31 | 系统状态 | 只读 | 0=空闲, 1=处理中, 2=完成 |

### 依赖文件

运行时必须包含：
- `leechcore.dll`
- `FTD3XX.dll`

---

**Version**: v1.0 | **Last Updated**: 2025-12-19
