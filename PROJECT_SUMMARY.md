# eRob CANopen Motor Control System

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![CANopen](https://img.shields.io/badge/Protocol-CANopen-green.svg)](https://www.can-cia.org/can-knowledge/canopen/)
[![CiA402](https://img.shields.io/badge/Standard-CiA402-orange.svg)](https://www.can-cia.org/can-knowledge/canopen/cia-402/)

A comprehensive **CANopen-based motor control system** for eRob actuator （ZeroErr Inc.）, featuring real-time position control, device discovery, and interactive motor management.

## 🚀 Quick Start

```bash
# Build the system
git clone https://github.com/CANopenNode/CANopenNode.git
cd CANopenNode && mkdir build && cd build
cmake .. && make

# Control a motor
./bin/pp_mode_control 2

# Discover devices
./bin/quick_scan
```

## ✨ Key Features

- **🎯 Profile Position Mode (PP Mode)** - Precise servo motor control
- **🔍 Device Discovery** - Automatic CANopen device scanning
- **⚡ Real-time Control** - Interactive motor control interface
- **🛡️ Safety Features** - Emergency stop and error handling
- **🔧 Easy Configuration** - Manual or interactive node ID setup
- **📡 CANopen Compliance** - Full CiA402 standard implementation

## 🏗️ System Architecture

```
Application Layer    → pp_mode_control, quick_scan, canopennode_blank
CANopenNode Stack   → SDO, PDO, NMT, Emergency handling
CAN Bus Interface   → SocketCAN driver
Hardware Layer      → eRob servo motors, CAN bus network
```

## 📋 Motor Control Commands

| Command | Description | Example |
|---------|-------------|---------|
| `p <pos>` | Move to position | `p 1000` |
| `v <vel>` | Set velocity | `v 2000` |
| `a <acc>` | Set acceleration | `a 3000` |
| `s` | Stop motor | `s` |
| `q` | Quit safely | `q` |

## 📚 Documentation

- **[Project Description](PROJECT_DESCRIPTION.md)** - Complete system overview
- **[Usage Examples](USAGE_EXAMPLES.md)** - Detailed usage guide
- **[Motor ID Guide](MOTOR_ID_GUIDE.md)** - Configuration instructions
- **[CANopenNode Docs](https://canopennode.github.io)** - Protocol documentation

## 🔧 Hardware Requirements

- Linux system with SocketCAN support
- CAN interface (CAN-USB adapter, CAN card)
- eRob servo motor with CANopen support
- CAN bus cable and termination resistors

## 🛠️ Software Requirements

- GCC compiler
- CMake 3.10+
- Linux kernel with CAN support
- SocketCAN tools (optional)

## 📖 Example Usage

```bash
# Interactive mode
$ ./bin/pp_mode_control
Usage: ./bin/pp_mode_control [node_id]
Example: ./bin/pp_mode_control 2
Please specify motor node ID (1-127): 2
Using motor node ID: 2

# Direct mode
$ ./bin/pp_mode_control 2
Using specified motor node ID: 2
=== PP mode initialization completed ===
>>> Enter command: p 1000
```

## 🤝 Contributing

Contributions are welcome! Please see our [Contributing Guidelines](CONTRIBUTING.md) for details.

## 📄 License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- [CANopenNode](https://github.com/CANopenNode/CANopenNode) - Open-source CANopen implementation
- [eRob Team](https://erob.com) - Motor specifications and testing
- [CiA](https://www.can-cia.org) - CANopen standards and documentation

---

**Ready to control your eRob motors?** Check out the [Quick Start Guide](#-quick-start) and [Usage Examples](USAGE_EXAMPLES.md)!
