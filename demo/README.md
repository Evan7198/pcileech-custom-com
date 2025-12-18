[English](README.md) | [中文](README_zh.md)

# PCILeech Demo Programs Directory

This directory contains two demo programs for the PCILeech FPGA custom register system.

### Demo Programs

#### 1. ACTIVATOR_DEMO - DNA Verification Activator

**Purpose**: FPGA DNA hardware identity verification and TLP access control activation

**Features**:
- Read FPGA hardware DNA value (57-bit unique identifier)
- Execute DNA encryption verification flow (Custom XOR algorithm)
- Enable TLP access control (register 0-23 access permission)
- Provide LeechCore API wrapper and utility functions

**Execution Order**: Must run this program first for DNA activation

**Documentation**: [ACTIVATOR_DEMO/README.md](./ACTIVATOR_DEMO/README.md)

---

#### 2. CUSTOMREGS_DEMO - Custom Register Demo

**Purpose**: Demonstrate how to use LeechCore API for custom register read/write operations

**Features**:
- Demonstrate basic register read/write (registers 0-23)
- Demonstrate multi-register independence verification
- Demonstrate DNA verification register access (registers 24-31)
- Complete error handling and resource cleanup examples

**Execution Order**: Run after ACTIVATOR_DEMO activation succeeds

**Documentation**: [CUSTOMREGS_DEMO/README.md](./CUSTOMREGS_DEMO/README.md)

---

### Quick Start

#### Step 1: DNA Activation (Must Execute First!)

```bash
cd ACTIVATOR_DEMO/x64/Release/
activator.exe
```

---

#### Step 2: Run Custom Register Demo

```bash
cd CUSTOMREGS_DEMO/x64/Release/
demo.exe
```

---

### Important Notes

#### Must Execute DNA Activation First

The TLP access control in FPGA firmware is disabled before DNA verification succeeds. If you run CUSTOMREGS_DEMO directly without activation first, accessing registers 0-23 will return all zeros or fail.

**Correct Usage Order**:
1. Run `ACTIVATOR_DEMO/activator.exe` for DNA activation
2. Confirm TLP control status is "Enabled"
3. Run `CUSTOMREGS_DEMO/demo.exe` for register operations

#### Register Groups

- **Registers 0-23**: General-purpose custom registers (requires DNA activation before access)
- **Registers 24-31**: DNA verification registers (readable before activation, but TLP function requires activation)

---

### Development Environment Requirements

#### Hardware
- PCILeech FPGA device (Artix-7 35T/75T/100T)
- USB 3.0 interface (FT601 chip)
- Target host (for PCIe testing)

#### Software
- Windows 10/11 x64
- Visual Studio 2022 (MSVC v143 toolset)
- FTD3XX driver installed

---

### Related Documentation

- [PCILeech Architecture & Communication Principles](../PCILeech架构与通信原理.md) - System architecture and software-hardware communication mechanism
- [Project Overview](../README.md) - Project introduction and documentation navigation

---

**Last Updated**: 2025-11-29
