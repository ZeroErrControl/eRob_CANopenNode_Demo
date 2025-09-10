# CANopen Profile Position Mode Control Program

A comprehensive CANopen Profile Position Mode (PP Mode) control program for servo motors, based on CiA402 standard and eRob CANopen and EtherCAT User Manual V1.9.

## Features

- **Automatic EDS File Parsing**: Automatically parses EDS (Electronic Data Sheet) files to extract object dictionary information
- **SDO Communication**: Uses Service Data Object (SDO) communication for parameter configuration
- **Profile Position Mode Control**: Implements immediate update mode for precise position control
- **Real-time Monitoring**: Provides real-time position monitoring and status checking
- **Interactive Control**: Offers an intuitive keyboard-based control interface
- **Parameter Management**: Supports control of position, velocity, acceleration, and deceleration

## Hardware Requirements

- Linux system with SocketCAN support
- CAN interface (e.g., CAN-USB adapter, CAN card)
- Servo motor compatible with CANopen protocol
- Motor node ID: 2 (configurable in source code)

## Software Requirements

- GCC compiler
- Linux kernel with CAN support
- SocketCAN tools (optional, for testing)

## Installation

1. Clone or download the CANopenNode repository
2. Navigate to the example directory:
   ```bash
   cd /path/to/CANopenNode/example
   ```

3. Compile the program:
   ```bash
   gcc -o pp_mode_control pp_mode_control.c -Wall -Wextra -g -pthread
   ```

## Configuration

### CAN Interface Setup

1. Bring up the CAN interface:
   ```bash
   sudo ip link set can0 type can bitrate 1000000
   sudo ip link set up can0
   ```

2. Verify the interface is up:
   ```bash
   ip link show can0
   ```

### EDS File Configuration

The program uses the EDS file `ZeroErr Driver_V1.5.eds` by default. Make sure this file is present in the example directory.

### Motor Parameters

- **Motor Resolution**: 524,288 counts per revolution
- **Position Range**: ±2 revolutions (±1,048,576 counts)
- **Default Velocity**: 5,566 counts/s
- **Default Acceleration**: 5,566 counts/s²
- **Default Deceleration**: 5,566 counts/s²

## Usage

### Starting the Program

1. Run the program:
   ```bash
   sudo ./pp_mode_control
   ```

2. The program will:
   - Initialize the CAN interface
   - Parse the EDS file
   - Initialize the motor in Profile Position Mode
   - Display the control interface

### Control Commands

| Command | Description | Example |
|---------|-------------|---------|
| `p <position>` | Set target position | `p 100000` |
| `v <velocity>` | Set profile velocity | `v 10000` |
| `a <acceleration>` | Set profile acceleration | `a 5000` |
| `d <deceleration>` | Set profile deceleration | `d 5000` |
| `+v` | Increase velocity by 100 | `+v` |
| `-v` | Decrease velocity by 100 | `-v` |
| `+a` | Increase acceleration by 100 | `+a` |
| `-a` | Decrease acceleration by 100 | `-a` |
| `+d` | Increase deceleration by 100 | `+d` |
| `-d` | Decrease deceleration by 100 | `-d` |
| `s` | Stop motor | `s` |
| `q` | Quit program | `q` |

### Position Control Examples

- **Move to position 0**: `p 0`
- **Move 1 revolution forward**: `p 524288`
- **Move 0.5 revolution backward**: `p -262144`
- **Move 2 revolutions forward**: `p 1048576`

### Real-time Status Display

The program displays current parameters in real-time:
```
=== Keyboard Control Instructions ===
p <position>     - Set target position (Current position: 12345, 0.02 rev)
v <velocity>     - Set profile velocity (Current: 5566)
a <acceleration> - Set profile acceleration (Current: 5566)
d <deceleration> - Set profile deceleration (Current: 5566)
...
```

## Technical Details

### CANopen Communication

The program implements the following CANopen objects:

| Object | Index | Description |
|--------|-------|-------------|
| Controlword | 0x6040 | Motor control commands |
| Statusword | 0x6041 | Motor status information |
| Modes of Operation | 0x6060 | Operation mode setting |
| Target Position | 0x607A | Target position for movement |
| Actual Position | 0x6064 | Current motor position |
| Profile Velocity | 0x6081 | Velocity profile parameter |
| Profile Acceleration | 0x6083 | Acceleration profile parameter |
| Profile Deceleration | 0x6084 | Deceleration profile parameter |

### Immediate Update Mode

The program uses immediate update mode for position control:

1. **Update Parameters**: Set target position and profile parameters
2. **Set Controlword bit4=1**: Indicate new position command
3. **Wait for Statusword bit12=1**: Confirm command received
4. **Set Controlword bit4=0**: Release position command data
5. **Wait for Statusword bit12=0**: Confirm ready for new commands

### Error Handling

- **CAN Communication Errors**: Automatic retry with timeout
- **Position Range Validation**: Prevents out-of-range movements
- **Motor Status Monitoring**: Real-time status checking
- **Graceful Shutdown**: Proper cleanup on program exit

## Troubleshooting

### Common Issues

1. **Permission Denied**: Run with `sudo` for CAN interface access
2. **CAN Interface Not Found**: Check if CAN interface is properly configured
3. **Motor Not Responding**: Verify motor node ID and CAN bus connection
4. **Position Not Updating**: Check motor status and error conditions

### Debug Information

The program provides detailed debug output:
- SDO request/response messages
- Motor status information
- Position change monitoring
- Error condition reporting

### CAN Bus Monitoring

Monitor CAN traffic using candump:
```bash
candump can0
```

## Safety Considerations

- **Emergency Stop**: Use `s` command to stop motor immediately
- **Position Limits**: Program enforces ±2 revolution limits
- **Velocity Limits**: Monitor velocity settings for safety
- **Power Supply**: Ensure adequate power supply for motor

## License

This program is part of the CANopenNode project. Please refer to the main project license.

## Support

For technical support and questions:
- Check the CANopenNode documentation
- Review the CiA402 standard
- Consult the motor manufacturer's documentation

## Version History

- **v1.0**: Initial release with basic PP mode control

