# Usage Examples

## Quick Start

### 1. Build the Project

```bash
cd /home/erobman/ecosystem/CANopenNode
mkdir build && cd build
cmake .. && make
```

### 2. Run PP Mode Control

**Method 1: Command Line Parameter (Recommended)**
```bash
./bin/pp_mode_control 2    # Use motor node ID 2
./bin/pp_mode_control 3    # Use motor node ID 3
./bin/pp_mode_control 5    # Use motor node ID 5
```

**Method 2: Interactive Input**
```bash
./bin/pp_mode_control
# Program will prompt: "Please specify motor node ID (1-127): "
# Enter your desired node ID and press Enter
```

### 3. Scan for Available Devices

```bash
# Quick scan all devices
./bin/quick_scan

# Scan specific range
./bin/quick_scan can0 50

# Get detailed info about a specific node
./bin/quick_scan read 2
```

## Program Output Examples

### Successful Connection
```bash
$ ./bin/pp_mode_control 2
eRob joint motor PP mode control program
Mode: Profile Position Mode (PP Mode)
Communication: CANopen SDO
Based on: eRob CANopen and EtherCAT User Manual V1.9

Using specified motor node ID: 2
EDS file parsed, loaded 100 objects
=== Check CAN connection ===
CAN connection normal, start motor control
=== Initialize PP mode (profile position mode) ===
1. Stop node...
2. Start node...
3. Set to profile position mode...
Write 0x6060:0 = 0x00000001... Success
4. Set profile parameters...
    Set profile velocity...
Write 0x6081:0 = 0x000015BE... Success
    Set profile acceleration...
Write 0x6083:0 = 0x000015BE... Success
    Set profile deceleration...
Write 0x6084:0 = 0x000015BE... Success
5. Clear error state...
    Control word=128 (Clear error)
Write 0x6040:0 = 0x00000080... Success
6. Motor enable sequence...
    Control word=6 (Shutdown)
Write 0x6040:0 = 0x00000006... Success
    Control word=7 (Switch on)
Write 0x6040:0 = 0x00000007... Success
    Control word=15 (Enable operation)
Write 0x6040:0 = 0x0000000F... Success
=== PP mode initialization completed ===

=== Keyboard control instructions ===
p <position>     - Set target position
v <velocity>     - Set profile velocity
a <acceleration> - Set profile acceleration
d <deceleration> - Set profile deceleration
+v              - Increase profile velocity (+100)
-v              - Decrease profile velocity (-100)
+a              - Increase profile acceleration (+100)
-a              - Decrease profile acceleration (-100)
+d              - Increase profile deceleration (+100)
-d              - Decrease profile deceleration (-100)
s               - Stop motor
q               - Exit program
==================

>>> Enter command: 
```

### Interactive Input Mode
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

### Error Handling
```bash
$ ./bin/pp_mode_control 200
eRob joint motor PP mode control program
Mode: Profile Position Mode (PP Mode)
Communication: CANopen SDO
Based on: eRob CANopen and EtherCAT User Manual V1.9

Error: node ID must be between 1 and 127
Usage: ./bin/pp_mode_control [node_id]
Example: ./bin/pp_mode_control 2
```

## Motor Control Commands

Once the program is running, you can use these commands:

### Position Control
```bash
p 1000          # Move to position 1000
p -500          # Move to position -500
p               # Show current target position
```

### Velocity Control
```bash
v 2000          # Set profile velocity to 2000
v               # Show current velocity
+v              # Increase velocity by 100
-v              # Decrease velocity by 100
```

### Acceleration/Deceleration Control
```bash
a 3000          # Set acceleration to 3000
d 2500          # Set deceleration to 2500
+a              # Increase acceleration by 100
-a              # Decrease acceleration by 100
+d              # Increase deceleration by 100
-d              # Decrease deceleration by 100
```

### Motor Control
```bash
s               # Stop motor immediately
q               # Quit program (safely stops motor)
```

## Troubleshooting

### 1. CAN Interface Issues
```bash
# Check if CAN interface exists
ip link show can0

# Check CAN interface status
ip link show can0 | grep UP

# Monitor CAN traffic
candump can0
```

### 2. Permission Issues
```bash
# Run with sudo if needed
sudo ./bin/pp_mode_control 2
```

### 3. Motor Not Responding
```bash
# First scan for available devices
./bin/quick_scan

# Check if motor is powered on
# Verify motor node ID
# Check CAN bus connection
```

## Advanced Usage

### Custom EDS File
The program automatically loads the EDS file from:
```
/home/erobman/ecosystem/CANopenNode/example/ZeroErr Driver_V1.5.eds
```

### Debug Mode
The program includes debug output showing all CAN messages:
```
[DEBUG] Send SDO: COB-ID=0x602, data: 2F 60 60 00 01 00 00 00
[DEBUG] Receive CAN: COB-ID=0x582, data: 60 60 60 00 00 00 00 00
```

### Multiple Motors
To control multiple motors, run multiple instances:
```bash
# Terminal 1
./bin/pp_mode_control 2

# Terminal 2  
./bin/pp_mode_control 3

# Terminal 3
./bin/pp_mode_control 4
```
