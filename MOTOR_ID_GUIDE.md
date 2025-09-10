# Motor Node ID Configuration Guide

## Overview

CANopenNode project now supports manual motor node ID input, simple and easy to use.

## Motor ID Configuration Methods

### 1. Command Line Parameter Mode (Recommended)

Specify motor node ID directly through command line parameters:

```bash
cd /home/erobman/ecosystem/CANopenNode/build
./bin/pp_mode_control 2    # Use node ID 2
./bin/pp_mode_control 3    # Use node ID 3
./bin/pp_mode_control 5    # Use node ID 5
```

### 2. Interactive Input Mode

If no parameters are provided, the program will prompt you to input the node ID:

```bash
cd /home/erobman/ecosystem/CANopenNode/build
./bin/pp_mode_control
```

**Interactive input process:**
- Program displays usage instructions on startup
- Prompts "Please specify motor node ID (1-127): "
- Input node ID and press Enter
- Program validates input range (1-127)
- If input is invalid, uses default node ID 2

### 3. Manual Specification Mode

If you need to modify the default node ID, you can edit the source code:

```c
// In pp_mode_control.c
#define MOTOR_NODE_ID 3  // Change to your desired default node ID
```

Then recompile:
```bash
cd /home/erobman/ecosystem/CANopenNode/build
make pp_mode_control
```

### 4. Using quick_scan Tool

Before running pp_mode_control, you can use the quick_scan tool to scan CAN bus devices:

```bash
# Quick scan all devices
./bin/quick_scan

# Scan specified range of nodes
./bin/quick_scan can0 50

# View detailed information of specific node
./bin/quick_scan read 2
```

## Supported Motor Device Types

The program automatically recognizes the following CiA402 device types:
- `0x00020192` - Standard CiA402 motor
- `0x00020193` - CiA402 motor with encoder
- `0x00020194` - CiA402 motor with absolute encoder
- `0x00020195` - CiA402 motor with multi-turn absolute encoder

## Troubleshooting

### 1. Motor Device Not Found

**Possible causes:**
- Motor not powered on
- Motor node ID not in range 1-20
- Motor baud rate not 1Mbps
- CAN connection issues

**Solutions:**
```bash
# Expand scan range
# Modify scan range in pp_mode_control.c
for (uint8_t node_id = 1; node_id <= 50; node_id++) {  // Change to 50

# Check CAN interface
ip link show can0
candump can0

# Use quick_scan tool
./bin/quick_scan can0 50
```

### 2. Motor Responds But Type Mismatch

**Symptom:** Program finds device but shows "device type mismatch"

**Solutions:**
- Check if motor supports CiA402 protocol
- Confirm motor is properly configured for CiA402 mode
- Check motor manual for device type code

### 3. Communication Timeout

**Symptom:** Program shows "no response" or "timeout"

**Solutions:**
- Check CAN bus connection
- Confirm motor baud rate settings
- Check if motor is online
- Try restarting motor

## Program Output Examples

### Using Command Line Parameters
```bash
$ ./bin/pp_mode_control 2
eRob joint motor PP mode control program
Mode: Profile Position Mode (PP Mode)
Communication: CANopen SDO
Based on: eRob CANopen and EtherCAT User Manual V1.9

Using specified motor node ID: 2
```

### Using Interactive Input
```bash
$ ./bin/pp_mode_control
eRob joint motor PP mode control program
Mode: Profile Position Mode (PP Mode)
Communication: CANopen SDO
Based on: eRob CANopen and EtherCAT User Manual V1.9

Usage: ./bin/pp_mode_control [node_id]
Example: ./bin/pp_mode_control 2
Please specify motor node ID (1-127): 3
Using motor node ID: 3
```

### Invalid Parameter Input
```bash
$ ./bin/pp_mode_control 200
eRob joint motor PP mode control program
Mode: Profile Position Mode (PP Mode)
Communication: CANopen SDO
Based on: eRob CANopen and EtherCAT User Manual V1.9

Error: Node ID must be in range 1-127
Usage: ./bin/pp_mode_control [node_id]
Example: ./bin/pp_mode_control 2
```

## Technical Details

### Manual Input Principle

1. **Command Line Parsing**: Program parses command line arguments for node ID
2. **Input Validation**: Validates node ID is in valid range (1-127)
3. **Interactive Fallback**: If no parameters, prompts user for input
4. **Default Fallback**: Uses default node ID if input is invalid

### Dynamic COB ID

Program uses dynamic COB ID to communicate with specified motor:
- SDO client COB ID: `0x600 + node_id`
- SDO server COB ID: `0x580 + node_id`
- NMT commands: Direct use of node ID

### Configuration Parameters

- **Input Range**: 1-127 (CANopen standard)
- **Default Node ID**: 2 (when no input provided)
- **Validation**: Strict range checking
- **Error Handling**: Clear error messages for invalid input

## Update Log

- **v1.0**: Added manual input functionality
- **v1.1**: Added command line parameter support
- **v1.2**: Added interactive input mode
- **v1.3**: Improved input validation and error handling

## Quick Start

1. **Build the program:**
   ```bash
   cd /home/erobman/ecosystem/CANopenNode
   mkdir build && cd build
   cmake .. && make
   ```

2. **Run with specific node ID:**
   ```bash
   ./bin/pp_mode_control 2
   ```

3. **Run with interactive input:**
   ```bash
   ./bin/pp_mode_control
   # Then input your desired node ID when prompted
   ```

4. **Scan for available devices:**
   ```bash
   ./bin/quick_scan
   ```