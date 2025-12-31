[English](README.md) | [中文](README_zh.md)

<a href="https://lattecode.cn" target="_blank">
  <img src="https://img.shields.io/badge/Sponsor-LatteCode-FFDD00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black" alt="LatteCode Sponsor">
</a>

# PCILeech Custom Register Communication System

A custom register communication system implementation for PCILeech FPGA, focusing on core DNA security verification and register communication functionality. Built on Xilinx Artix-7 FPGA with high-speed USB 3.0 (FT601) communication, supporting 57-bit hardware DNA-based identity verification.

## Features

- **32-Register Architecture**: 24 general-purpose registers (0-23) + 8 DNA verification registers (24-31)
- **FPGA DNA Verification**: 57-bit hardware unique identifier based authentication
- **TLP Access Control**: Register access control based on DNA verification
- **USB 3.0 High-Speed**: FT601 chip supporting up to 400MB/s transfer rate
- **Custom LeechCore API**: Extended API for seamless register read/write operations

## Quick Start

```bash
# 1. Replace file pcileech_fifo.sv file
Replace the pcileech_fifo.sv file

# 2. Compile
Compile software and hardware projects

# 3. Hardware Connection
Connect PCILeech FPGA device via USB 3.0

# 4. DNA Activation (Required First!)
cd demo/ACTIVATOR_DEMO/x64/Release
activator.exe

# 5. Run Register Demo
cd demo/CUSTOMREGS_DEMO/x64/Release
demo.exe
```

## Documentation

- **[Architecture & Communication Principles](./PCILeech架构与通信原理.md)** - System architecture and software-hardware communication mechanism
- **[Demo Programs Overview](./demo/README.md)** - Usage guide for two demo programs
- **[DNA Activator](./demo/ACTIVATOR_DEMO/README.md)** - DNA verification activator usage guide
- **[Custom Register Demo](./demo/CUSTOMREGS_DEMO/README.md)** - API usage standard examples

## Support

**Discord Community**: [Join our Discord](https://discord.gg/PwAXYPMkkF)

## License

Based on PCILeech & LeechCore open-source project, following the original project's license.

**Version**: v1.0
**Last Updated**: 2025-12-19
