[English](README.md) | [中文](README_zh.md)

# PCILeech FPGA DNA Activator - DEMO

### Introduction

This is a **simple DEMO program** demonstrating how to use LeechCore API for FPGA DNA verification and activation.

**Project Type**: DNA Verification Demo Program
**Compiler**: Visual Studio 2022 (MSVC v143)
**Target Platform**: Windows x64
**Version**: v1.0 DEMO

### Features

- ✅ **Concise Code**: Single main.c + utility functions
- ✅ **Detailed Comments**: Complete English/Chinese annotations for easy understanding
- ✅ **Clear Steps**: 5-step complete activation workflow demo
- ✅ **Easy to Extend**: Core utility functions retained for secondary development

### Quick Start

#### Compile and Run

```bash
# 1. Open Visual Studio 2022
Double-click activator.sln

# 2. Select Release | x64 configuration

# 3. Press F7 to compile

# 4. Run
cd x64\Release
activator.exe -v
```

#### Expected Output

```
====================================================
  PCILeech FPGA DNA Activator v1.0 DEMO
====================================================

Step 1: Connect FPGA Device → [PASS]
Step 2: Read FPGA DNA Value → [PASS]
Step 3: Execute DNA Verification Flow → [PASS]
Step 4: Check TLP Control Status → [PASS]
Step 5: Activation Summary → ✅ Success

====================================================
  ✓ DNA Activation Demo Completed
====================================================
```

### Core Features

#### Demo Steps

1. **Connect FPGA Device** - Initialize LeechCore
2. **Read DNA Value** - Read 57-bit hardware unique identifier (registers 24-25)
3. **Execute Verification Flow** - Custom XOR encryption verification (registers 26-31)
4. **Check TLP Status** - Verify access control (register 29)
5. **Display Activation Summary** - Complete activation information

#### API Usage Example

```c
// 1. Connect device
HANDLE hLC = init_leechcore("fpga");

// 2. Read DNA
uint64_t dna = read_dna_value(hLC);

// 3. Start verification
start_verification(hLC);

// 4. Read encrypted value and decrypt
uint32_t enc = read_encrypted_value(hLC);
uint32_t dec = dna_decrypt(enc, dna);
write_decrypted_result(hLC, dec);

// 5. Check status
int status = check_verification_status(hLC);
int tlp = check_tlp_control_status(hLC);

// 6. Cleanup
cleanup_leechcore(hLC);
```

### Register Mapping (24-31)

| Register | Function | Read/Write | Description |
|----------|----------|-----------|-------------|
| 24-25 | DNA Value (57-bit) | Read-only | FPGA hardware DNA |
| 26 | FPGA Encrypted Value | Read-only | Auto-generated encrypted random number |
| 27 | Decryption Result | Write-only | Software decryption result |
| 28 | Verification Status | Read-only | 0=Failed, 1=Success |
| 29 | TLP Control | Read-only | 0=Disabled, 1=Enabled |
| 30 | Control Command | Write-only | 1=Start verification |
| 31 | System Status | Read-only | 0=Idle, 1=Processing, 2=Complete |

### Dependencies

Runtime required files:
- `leechcore.dll`
- `FTD3XX.dll`

---

**Version**: v1.0 | **Last Updated**: 2025-11-28
