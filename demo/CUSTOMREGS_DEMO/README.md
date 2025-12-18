[English](README.md) | [中文](README_zh.md)

# PCILeech Custom Register DEMO

This directory contains standard API usage examples for the PCILeech custom register system, demonstrating FPGA DNA verification activation and register read/write operations.

### Quick Start

#### 1. Environment Requirements

**Hardware**:
- PCILeech FPGA device (Artix-7 35T/75T/100T)
- FT601 USB 3.0 interface

**Software**:
- Windows 10/11 x64
- Visual Studio 2022
- FTD3XX driver installed

#### 2. Compile

1. Double-click to open `demo.sln`
2. Select configuration in Visual Studio 2022 (Debug or Release)
3. Press `F5` or `Ctrl+Shift+B` to compile

#### 3. Run

- **In Visual Studio**: Press `F5` to run and debug
- **Standalone run**: `x64/Release/demo.exe`

### API Usage Guide

#### Register Read/Write API

```c
#include "leechcore.h"

// Custom command definitions
#define LC_CMD_FPGA_CUSTOM_READ_REG(n)  (0x0200000000000000 | ((n) & 0xFF))
#define LC_CMD_FPGA_CUSTOM_WRITE_REG(n) (0x0201000000000000 | ((n) & 0xFF))

HANDLE hLC = LcCreateEx(&cfg, NULL);

// Read register 5
PBYTE pbDataOut = NULL;
DWORD cbDataOut = 0;
if (LcCommand(hLC, LC_CMD_FPGA_CUSTOM_READ_REG(5), 0, NULL, &pbDataOut, &cbDataOut)) {
    DWORD value = *(PDWORD)pbDataOut;
    printf("Register 5 = 0x%08X\n", value);
    LocalFree(pbDataOut);
}

// Write register 10
DWORD value = 0xDEADBEEF;
LcCommand(hLC, LC_CMD_FPGA_CUSTOM_WRITE_REG(10), sizeof(DWORD), (PBYTE)&value, NULL, NULL);

LcClose(hLC);
```

### Register Mapping Table (32 Registers)

| Register | Purpose | Read/Write | Description |
|----------|---------|-----------|-------------|
| 0-23 | General-purpose custom registers | R/W | User available, requires DNA activation for access |
| 24 | DNA value low 32-bit | R | FPGA hardware DNA low 32 bits |
| 25 | DNA value high 25-bit | R | FPGA hardware DNA high 25 bits |
| 26 | Encrypted random value | R | FPGA-generated random number (for verification) |
| 27 | Decryption result | W | Software-written decryption result |
| 28 | Verification status | R | DNA verification status (0=Failed, 1=Success) |
| 29 | TLP control status | R | TLP access control (0=Disabled, 1=Enabled) |
| 30 | Control command | W | Control command (1=Start verification) |
| 31 | System status | R | System status (0=Idle, 1=Processing, 2=Complete) |

### Important Notes

#### Must Execute DNA Activation First!

**Reason**:
- FPGA firmware TLP access control is disabled before DNA verification succeeds
- Accessing registers 0-23 without activation will return all zeros or fail
- Registers 24-31 (DNA verification area) can be read before activation, but TLP function unavailable

**Correct Usage Order**:
1. ✅ Connect FPGA device (`LcCreateEx`)
2. ✅ Read DNA value (Registers 24-25)
3. ✅ Check TLP status (Register 29)
4. ✅ Confirm TLP enabled
5. ✅ Perform register operations (0-23)

### Troubleshooting

#### Issue 1: "Unable to connect to FPGA device"

**Possible causes**:
- FPGA device not connected
- FTD3XX driver not installed
- DLL files missing

**Solutions**:
1. Check FPGA is connected via USB
2. Confirm FT601 device exists in Device Manager
3. Confirm `leechcore.dll` and `FTD3XX.dll` are in executable directory

#### Issue 2: Still unable to access registers after DNA activation

**Possible causes**:
- FPGA firmware did not correctly implement DNA verification logic
- TLP control not correctly enabled
- Register 29 value incorrect

**Solutions**:
1. Read register 29, confirm value is `0x00000001`
2. Check FPGA firmware DNA verification state machine
3. Confirm firmware version matches software

---

**Project Status**: ✅ Core functionality complete | **Register Count**: 32 (0-31)
