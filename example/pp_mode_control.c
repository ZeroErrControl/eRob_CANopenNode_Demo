/*
 * author: ZeroErr Inc.
 * CANopen Profile Position Mode (PP Mode) Control Program
 * Based on CiA402 standard and eRob CANopen and EtherCAT User Manual V1.9
 * Uses SDO communication for PP mode control
 * 
 * Features:
 * - Automatic EDS file parsing for object dictionary
 * - SDO communication for parameter configuration
 * - Profile Position Mode motor control with immediate update mode
 * - Real-time position monitoring and status checking
 * - Interactive keyboard control interface
 * - Support for position, velocity, acceleration, and deceleration control
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <termios.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

// Configuration constants
#define TIMEOUT_MS 1000
#define MOTOR_NODE_ID 2  // Default motor node ID, can be overridden by auto-detection
#define SDO_CLIENT_COB_ID (0x600 + MOTOR_NODE_ID)  // 0x602 for node 2
#define SDO_SERVER_COB_ID (0x580 + MOTOR_NODE_ID)  // 0x582 for node 2

// Auto-detection variables
static uint8_t detected_motor_id = 0;  // 0 means not detected yet
static uint8_t current_motor_id = MOTOR_NODE_ID;  // Currently used motor ID
//#define EDS_FILE_PATH "/home/erobman/ecosystem/CANopenNode/example/ZeroErr_Driver_V1.5.xpd"
#define EDS_FILE_PATH "/home/erobman/ecosystem/CANopenNode/example/ZeroErr Driver_V1.5.eds"

// Global control variables
volatile int running = 1;

// Motion parameters
int32_t current_target_position = 0;
uint32_t current_profile_velocity = 5566;
uint32_t current_profile_acceleration = 5566;
uint32_t current_profile_deceleration = 5566;
int motor_enabled = 0; // Flag indicating if motor is enabled

// Motor parameters
#define MOTOR_RESOLUTION 524288  // Resolution per revolution
#define MAX_POSITION (MOTOR_RESOLUTION * 2)  // Maximum position: 2 revolutions
#define MIN_POSITION (-MOTOR_RESOLUTION * 2) // Minimum position: -2 revolutions

// Keyboard input related (thread removed)

// Function declarations
int execute_position_move(int sock, int32_t target_position);
void print_command_help(int sock);

/* Object dictionary entry structure */
typedef struct {
    uint16_t index;
    uint8_t subindex;
    uint8_t data_size;
} object_dict_entry_t;

/* Simplified object dictionary */
object_dict_entry_t object_dict[100];
int object_dict_count = 0;

/* Get byte size based on data type */
uint8_t get_data_type_size(uint16_t data_type) {
    switch (data_type) {
        case 0x0001: return 1;  // BOOLEAN
        case 0x0002: return 1;  // INTEGER8
        case 0x0003: return 2;  // INTEGER16
        case 0x0004: return 4;  // INTEGER32
        case 0x0005: return 1;  // UNSIGNED8
        case 0x0006: return 2;  // UNSIGNED16 (Control word type)
        case 0x0007: return 4;  // UNSIGNED32
        case 0x0008: return 8;  // REAL32
        default: return 2;      // Default 2 bytes
    }
}

/* 解析EDS文件 */
int parse_eds_file(const char* filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("无法打开EDS文件: %s\n", filename);
        return -1;
    }
    
    char line[256];
    uint16_t current_index = 0;
    uint8_t current_subindex = 0;
    uint16_t current_data_type = 0;
    int in_object = 0;
    
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;
        
        if (line[0] == '[' && strchr(line, ']')) {
            char *end_bracket = strchr(line, ']');
            *end_bracket = 0;
            char *object_id = line + 1;
            
            if (strstr(object_id, "sub")) {
                sscanf(object_id, "%hxsub%hhu", &current_index, &current_subindex);
            } else {
                sscanf(object_id, "%hx", &current_index);
                current_subindex = 0;
            }
            in_object = 1;
            current_data_type = 0;
        }
        else if (in_object && strstr(line, "DataType=")) {
            sscanf(line, "DataType=0x%hx", &current_data_type);
            // Special handling for control word, ensure correct data type
            if (current_index == 0x6040) {
                current_data_type = 0x0006; // UNSIGNED16
            }
        }
        else if (in_object && strstr(line, "AccessType=")) {
            if (current_data_type > 0 && object_dict_count < 100) {
                object_dict[object_dict_count].index = current_index;
                object_dict[object_dict_count].subindex = current_subindex;
                object_dict[object_dict_count].data_size = get_data_type_size(current_data_type);
                object_dict_count++;
            }
            in_object = 0;
        }
    }
    
    fclose(file);
    printf("EDS file parsed, loaded %d objects\n", object_dict_count);
    return 0;
}

/* Find object dictionary entry */
uint8_t get_object_size(uint16_t index, uint8_t subindex) {
    for (int i = 0; i < object_dict_count; i++) {
        if (object_dict[i].index == index && object_dict[i].subindex == subindex) {
            return object_dict[i].data_size;
        }
    }
    // Special handling for control word and mode     
    if (index == 0x6040) {
        return 2; // Control word is 2 bytes (using 2B)
    }
    if (index == 0x6060) {
        return 1; // Operation mode is 1 byte (using 2F)
    }
    if (index == 0x6081 || index == 0x6083 || index == 0x6084) {
        return 4; // Profile parameters are 4 bytes (using 23)
    }
    return 4; // Default 4 bytes
}

/* Send SDO request */
int send_sdo_request(int sock, uint16_t index, uint8_t subindex, uint32_t data, int is_write) {
    struct can_frame frame;
    frame.can_id = 0x600 + current_motor_id;  // Use dynamic motor ID
    frame.can_dlc = 8;
    
    uint8_t data_size = get_object_size(index, subindex);
    
    if (is_write) {
        if (data_size == 1) {
            frame.data[0] = 0x2F;  // 1 byte data
        } else if (data_size == 2) {
            frame.data[0] = 0x2B;  // 2 byte data
        } else {
            frame.data[0] = 0x23;  // 4 byte data
        }
        // Correct byte order: low byte first
        frame.data[1] = index & 0xFF;        // Index low byte
        frame.data[2] = (index >> 8) & 0xFF; // Index high byte
        frame.data[3] = subindex;            // Subindex
        frame.data[4] = data & 0xFF;         // Data low byte
        frame.data[5] = (data >> 8) & 0xFF;  // Data second low byte
        frame.data[6] = (data >> 16) & 0xFF; // Data second high byte
        frame.data[7] = (data >> 24) & 0xFF; // Data high byte
    } else {
        frame.data[0] = 0x40;  // Upload request
        frame.data[1] = index & 0xFF;
        frame.data[2] = (index >> 8) & 0xFF;
        frame.data[3] = subindex;
        frame.data[4] = 0;
        frame.data[5] = 0;
        frame.data[6] = 0;
        frame.data[7] = 0;
    }
    
    // Add debug output
    printf("[DEBUG] Send SDO: COB-ID=0x%03X, data: %02X %02X %02X %02X %02X %02X %02X %02X\n", 
           frame.can_id, frame.data[0], frame.data[1], frame.data[2], frame.data[3],
           frame.data[4], frame.data[5], frame.data[6], frame.data[7]);
    
    return write(sock, &frame, sizeof(frame));
}

/* Receive SDO response */
int receive_sdo_response(int sock, uint32_t *data) {
    struct can_frame frame;
    fd_set readfds;
    struct timeval timeout;
    int ret;
    clock_t start_time = clock();
    
    while ((clock() - start_time) * 1000 / CLOCKS_PER_SEC < TIMEOUT_MS) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000;
        
        ret = select(sock + 1, &readfds, NULL, NULL, &timeout);
        if (ret > 0) {
            ret = read(sock, &frame, sizeof(frame));
            if (ret > 0) {
                // Add debug output
                printf("[DEBUG] Receive CAN: COB-ID=0x%03X, data: %02X %02X %02X %02X %02X %02X %02X %02X\n", 
                       frame.can_id, frame.data[0], frame.data[1], frame.data[2], frame.data[3],
                       frame.data[4], frame.data[5], frame.data[6], frame.data[7]);
                
                if (frame.can_id == (0x580 + current_motor_id)) {  // Use dynamic motor ID
                    if ((frame.data[0] & 0xE0) == 0x40) {  // Upload response
                        *data = frame.data[4] | (frame.data[5] << 8) | 
                                (frame.data[6] << 16) | (frame.data[7] << 24);
                        printf("[DEBUG] SDO upload response: data=0x%08X\n", *data);
                        return 0;
                    } else if ((frame.data[0] & 0xE0) == 0x60) {  // Download response
                        *data = 0;
                        printf("[DEBUG] SDO download response: success\n");
                        return 0;
                    } else if ((frame.data[0] & 0xE0) == 0x80) {  // Error response
                        printf("[DEBUG] SDO error response: error code=0x%02X%02X%02X%02X\n", 
                               frame.data[4], frame.data[5], frame.data[6], frame.data[7]);
                        return -1;
                    }
                }
            }
        }
    }
    printf("[DEBUG] SDO response timeout\n");
    return -1;
}

/* Write SDO data */
int write_sdo(int sock, uint16_t index, uint8_t subindex, uint32_t data) {
    printf("Write 0x%04X:%d = 0x%08X... ", index, subindex, data);
    fflush(stdout);
    
    if (send_sdo_request(sock, index, subindex, data, 1) < 0) {
        printf("Send failed\n");
        return -1;
    }
    
    uint32_t response_data;
    if (receive_sdo_response(sock, &response_data) < 0) {
        printf("No response\n");
        return -1;
    }
    
    printf("Success\n");
    return 0;
}

/* Read SDO data */
int read_sdo(int sock, uint16_t index, uint8_t subindex, uint32_t *data) {
    printf("Read 0x%04X:%d... ", index, subindex);
    fflush(stdout);
    
    if (send_sdo_request(sock, index, subindex, 0, 0) < 0) {
        printf("Send failed\n");
        return -1;
    }
    
    if (receive_sdo_response(sock, data) < 0) {
        printf("No response\n");
        return -1;
    }
    
    printf("0x%08X\n", *data);
    return 0;
}

/* Auto-detect motor node ID */
int auto_detect_motor(int sock) {
    printf("正在自动检测电机节点ID...\n");
    
    for (uint8_t node_id = 1; node_id <= 20; node_id++) {
        printf("检测节点 %d... ", node_id);
        fflush(stdout);
        
        // 临时更新SDO COB ID
        uint32_t temp_client_cob = 0x600 + node_id;
        uint32_t temp_server_cob = 0x580 + node_id;
        
        // 发送SDO请求读取设备类型 (0x1000)
        struct can_frame frame;
        frame.can_id = temp_client_cob;
        frame.can_dlc = 8;
        frame.data[0] = 0x40;  // Upload request
        frame.data[1] = 0x00;  // Index low
        frame.data[2] = 0x10;  // Index high
        frame.data[3] = 0x00;  // Subindex
        frame.data[4] = 0;
        frame.data[5] = 0;
        frame.data[6] = 0;
        frame.data[7] = 0;
        
        if (write(sock, &frame, sizeof(frame)) < 0) {
            printf("发送失败\n");
            continue;
        }
        
        // 接收响应
        fd_set readfds;
        struct timeval timeout;
        int ret;
        clock_t start_time = clock();
        uint32_t device_type = 0;
        int found = 0;
        
        while ((clock() - start_time) * 1000 / CLOCKS_PER_SEC < 200) {  // 200ms timeout
            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);
            
            timeout.tv_sec = 0;
            timeout.tv_usec = 10000;  // 10ms
            
            ret = select(sock + 1, &readfds, NULL, NULL, &timeout);
            if (ret > 0) {
                ret = read(sock, &frame, sizeof(frame));
                if (ret > 0 && frame.can_id == temp_server_cob) {
                    if ((frame.data[0] & 0xE0) == 0x40) {  // Upload response
                        device_type = frame.data[4] | (frame.data[5] << 8) | 
                                     (frame.data[6] << 16) | (frame.data[7] << 24);
                        found = 1;
                        break;
                    }
                }
            }
        }
        
        if (found) {
            printf("找到设备 (类型: 0x%08X)\n", device_type);
            
            // 检查是否是电机设备 (CiA402设备类型通常是0x00020192)
            if (device_type == 0x00020192 || device_type == 0x00020193 || 
                device_type == 0x00020194 || device_type == 0x00020195) {
                printf("✓ 找到CiA402电机设备，节点ID: %d\n", node_id);
                detected_motor_id = node_id;
                current_motor_id = node_id;
                return node_id;
            } else {
                printf("设备类型不匹配 (期望CiA402电机)\n");
            }
        } else {
            printf("无响应\n");
        }
        
        usleep(50000);  // 50ms delay
    }
    
    printf("未找到电机设备，使用默认节点ID: %d\n", MOTOR_NODE_ID);
    detected_motor_id = 0;
    current_motor_id = MOTOR_NODE_ID;
    return MOTOR_NODE_ID;
}

/* Send NMT command */
int send_nmt_command(int sock, uint8_t command, uint8_t node_id) {
    struct can_frame frame;
    frame.can_id = 0x000;
    frame.can_dlc = 2;
    frame.data[0] = command;
    frame.data[1] = node_id;
    return write(sock, &frame, sizeof(frame));
}

/* Signal handler */
void signal_handler(int sig) {
    printf("\n\nProgram interrupted, stopping motor safely...\n");
    running = 0;
    exit(0);  // Force exit program
}

/* Set terminal to non-blocking mode */
void set_terminal_nonblocking() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO);
    ttystate.c_cc[VMIN] = 1;  // At least read 1 character
    ttystate.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

/* Restore terminal settings */
void restore_terminal() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

/* Keyboard input processing thread */
void* keyboard_input_thread(void* arg) {
    int sock = *(int*)arg;
    char input[256];
    
    printf("\n=== Keyboard control instructions ===\n");
    printf("p <position>     - Set target position\n");
    printf("v <velocity>     - Set profile velocity\n");
    printf("a <acceleration>   - Set profile acceleration\n");
    printf("d <deceleration>   - Set profile deceleration\n");
    printf("+v           - Increase profile velocity (+100)\n");
    printf("-v           - Decrease profile velocity (-100)\n");
    printf("+a           - Increase profile acceleration (+100)\n");
    printf("-a           - Decrease profile acceleration (-100)\n");
    printf("+d           - Increase profile deceleration (+100)\n");
    printf("-d           - Decrease profile deceleration (-100)\n");
    printf("s            - Stop motor\n");
    printf("q            - Exit program\n");
    printf("==================\n\n");
    
    while (running) {
        printf("\nEnter command: ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) != NULL) {
            // Remove newline
            input[strcspn(input, "\n")] = 0;
            
            if (strlen(input) == 0) continue;
            
            char cmd = input[0];
            int parsed_value = 0;
            
            if (strlen(input) > 2) {
                sscanf(input + 2, "%d", &parsed_value);
            }
            
            switch (cmd) {
                case 'p': // Set target position
                    if (strlen(input) > 2) {
                        // There is a position parameter, execute position movement
                        current_target_position = parsed_value;
                        printf("Set target position: %d\n", current_target_position);
                        execute_position_move(sock, current_target_position);
                    } else {
                        // There is no position parameter, display current target position
                        printf("Current target position: %d\n", current_target_position);
                    }
                    break;
                    
                case 'v': // Set profile velocity
                    if (parsed_value > 0) {
                        current_profile_velocity = parsed_value;
                        printf("Set profile velocity: %d\n", current_profile_velocity);
                        write_sdo(sock, 0x6081, 0, current_profile_velocity);
                    } else {
                        printf("Current profile velocity: %d\n", current_profile_velocity);
                    }
                    print_command_help(sock);
                    break;
                    
                case 'a': // Set profile acceleration
                    if (parsed_value > 0) {
                        current_profile_acceleration = parsed_value;
                        printf("Set profile acceleration: %d\n", current_profile_acceleration);
                        write_sdo(sock, 0x6083, 0, current_profile_acceleration);
                    } else {
                        printf("Current profile acceleration: %d\n", current_profile_acceleration);
                    }
                    print_command_help(sock);
                    break;
                    
                case 'd': // Set profile deceleration
                    if (parsed_value > 0) {
                        current_profile_deceleration = parsed_value;
                        printf("Set profile deceleration: %d\n", current_profile_deceleration);
                        write_sdo(sock, 0x6084, 0, current_profile_deceleration);
                    } else {
                        printf("Current profile deceleration: %d\n", current_profile_deceleration);
                    }
                    print_command_help(sock);
                    break;
                    
                case '+': // Increase parameter
                    if (strlen(input) >= 2) {
                        switch (input[1]) {
                            case 'v':
                                current_profile_velocity += 100;
                                printf("Increase profile velocity to: %d\n", current_profile_velocity);
                                write_sdo(sock, 0x6081, 0, current_profile_velocity);
                                break;
                            case 'a':
                                current_profile_acceleration += 100;
                                printf("Increase profile acceleration to: %d\n", current_profile_acceleration);
                                write_sdo(sock, 0x6083, 0, current_profile_acceleration);
                                break;
                            case 'd':
                                current_profile_deceleration += 100;
                                printf("Increase profile deceleration to: %d\n", current_profile_deceleration);
                                write_sdo(sock, 0x6084, 0, current_profile_deceleration);
                                break;
                        }
                    }
                    break;
                    
                case '-': // Decrease parameter
                    if (strlen(input) >= 2) {
                        switch (input[1]) {
                            case 'v':
                                if (current_profile_velocity > 100) {
                                    current_profile_velocity -= 100;
                                    printf("Decrease profile velocity to: %d\n", current_profile_velocity);
                                    write_sdo(sock, 0x6081, 0, current_profile_velocity);
                                } else {
                                    printf("Profile velocity cannot be less than 100\n");
                                }
                                break;
                            case 'a':
                                if (current_profile_acceleration > 100) {
                                    current_profile_acceleration -= 100;
                                    printf("Decrease profile acceleration to: %d\n", current_profile_acceleration);
                                    write_sdo(sock, 0x6083, 0, current_profile_acceleration);
                                } else {
                                    printf("Profile acceleration cannot be less than 100\n");
                                }
                                break;
                            case 'd':
                                if (current_profile_deceleration > 100) {
                                    current_profile_deceleration -= 100;
                                    printf("Decrease profile deceleration to: %d\n", current_profile_deceleration);
                                    write_sdo(sock, 0x6084, 0, current_profile_deceleration);
                                } else {
                                    printf("Profile deceleration cannot be less than 100\n");
                                }
                                break;
                        }
                    }
                    break;
                    
                case 's': // Stop motor
                    printf("Stop motor...\n");
                    write_sdo(sock, 0x6040, 0, 0x06); // Shutdown
                    print_command_help(sock);
                    break;
                    
                case 'q': // Exit program
                    printf("Exit program...\n");
    running = 0;
                    break;
                    
                default:
                    printf("Unknown command: %c\n", cmd);
                    break;
            }
        }
    }
    
    return NULL;
}

/* PP模式初始化 */
int init_pp_mode(int sock) {
    printf("=== Initialize PP mode (profile position mode) ===\n");
    
    // 1. Stop node
    printf("1. Stop node...\n");
    send_nmt_command(sock, 0x02, current_motor_id);  // Close node
    usleep(200000);
    send_nmt_command(sock, 0x82, current_motor_id);  // Reset node
    usleep(1000000);  // Increase waiting time
    
    // 2. Start node
    printf("2. Start node...\n");
    send_nmt_command(sock, 0x01, current_motor_id);
    usleep(1000000);  // Wait for node to start completely
    
    // 3. Set to profile position mode
    printf("3. Set to profile position mode...\n");
    if (write_sdo(sock, 0x6060, 0, 0x01) < 0) {
        printf("Set profile position mode failed, try again...\n");
    }
    usleep(200000);
    
    // 4. Set profile parameters
    printf("4. Set profile parameters...\n");
    printf("    Set profile velocity...\n");
    if (write_sdo(sock, 0x6081, 0, 5566) < 0) {
        printf("Set profile velocity failed, try again...\n");
    }
    usleep(200000);
    
    printf("    Set profile acceleration...\n");
    if (write_sdo(sock, 0x6083, 0, 5566) < 0) {
        printf("Set profile acceleration failed, try again...\n");
    }
    usleep(200000);
    
    printf("    Set profile deceleration...\n");
    if (write_sdo(sock, 0x6084, 0, 5566) < 0) {
        printf("Set profile deceleration failed, try again...\n");
    }
    usleep(200000);
    
    // 5. Clear error state
    printf("5. Clear error state...\n");
    printf("    Control word=128 (Clear error)\n");
    if (write_sdo(sock, 0x6040, 0, 0x80) < 0) {
        printf("Clear error command failed, try again...\n");
    }
    usleep(500000);
    
    // 6. Motor enable sequence (according to the correct sequence in the screenshot)
    printf("6. Motor enable sequence...\n");
    printf("    Control word=6 (Shutdown)\n");
    if (write_sdo(sock, 0x6040, 0, 0x06) < 0) {
        printf("Shutdown command failed, try again...\n");
    }
    usleep(500000);
    
    printf("    Control word=7 (Switch on)\n");
    if (write_sdo(sock, 0x6040, 0, 0x07) < 0) {
        printf("Switch on command failed, try again...\n");
    }
    usleep(500000);
    
    printf("    Control word=15 (Enable operation)\n");
    if (write_sdo(sock, 0x6040, 0, 0x0F) < 0) {
        printf("Enable operation command failed, try again...\n");
    }
    usleep(500000);
    
    printf("=== PP mode initialization completed ===\n");
    motor_enabled = 1; // Mark motor as enabled
    return 0;
}

/* Print command help with current parameters */
void print_command_help(int sock) {
    // Read current position
    uint32_t actual_position = 0;
    if (read_sdo(sock, 0x6064, 0, &actual_position) == 0) {
        float current_turns = (float)(int32_t)actual_position / MOTOR_RESOLUTION;
        printf("\n=== Keyboard control instructions ===\n");
        printf("p <position>     - Set target position (current position：%d, %.2f)\n", (int32_t)actual_position, current_turns);
        printf("v <velocity>     - Set profile velocity (current：%d)\n", current_profile_velocity);
        printf("a <acceleration>   - Set profile acceleration (current：%d)\n", current_profile_acceleration);
        printf("d <deceleration>   - Set profile deceleration (current：%d)\n", current_profile_deceleration);
        printf("+v           - Increase profile velocity (+100)\n");
        printf("-v           - Decrease profile velocity (-100)\n");
        printf("+a           - Increase profile acceleration (+100)\n");
        printf("-a           - Decrease profile acceleration (-100)\n");
        printf("+d           - Increase profile deceleration (+100)\n");
        printf("-d           - Decrease profile deceleration (-100)\n");
        printf("s            - Stop motor\n");
        printf("q            - Exit program\n");
        printf("==================\n");
    } else {
        printf("\n=== Keyboard control instructions ===\n");
        printf("p <position>     - Set target position (current position：read failed)\n");
        printf("v <velocity>     - Set profile velocity (current：%d)\n", current_profile_velocity);
        printf("a <acceleration>   - Set profile acceleration (current：%d)\n", current_profile_acceleration);
        printf("d <deceleration>   - Set profile deceleration (current：%d)\n", current_profile_deceleration);
        printf("+v           - Increase profile velocity (+100)\n");
        printf("-v           - Decrease profile velocity (-100)\n");
        printf("+a           - Increase profile acceleration (+100)\n");
        printf("-a           - Decrease profile acceleration (-100)\n");
        printf("+d           - Increase profile deceleration (+100)\n");
        printf("-d           - Decrease profile deceleration (-100)\n");
        printf("s            - Stop motor\n");
        printf("q            - Exit program\n");
        printf("==================\n");
    }
}

/* Execute position movement - Immediate update mode */
int execute_position_move(int sock, int32_t target_position) {
    printf("\n=== Execute position movement (immediate update mode) ===\n");
    
    // Check position range
    if (target_position > MAX_POSITION) {
        printf("Error: target position %d exceeds maximum range %d\n", target_position, MAX_POSITION);
        return -1;
    }
    if (target_position < MIN_POSITION) {
        printf("Error: target position %d exceeds minimum range %d\n", target_position, MIN_POSITION);
        return -1;
    }
    
    // Display position information (turns)
    float target_turns = (float)target_position / MOTOR_RESOLUTION;
    printf("Target position: %d (%.2f turns)\n", target_position, target_turns);
    
    // Read current position
    uint32_t current_position;
    if (read_sdo(sock, 0x6064, 0, &current_position) == 0) {
        float current_turns = (float)(int32_t)current_position / MOTOR_RESOLUTION;
        printf("Current position: %d (%.2f turns)\n", (int32_t)current_position, current_turns);
    }
    
    // 1. Update position parameters
    printf("1. Update position parameters...\n");
    
    // Set target position
    printf("    Set target position...\n");
    if (write_sdo(sock, 0x607A, 0, target_position) < 0) {
        printf("Set target position failed\n");
        return -1;
    }
    printf("    Success\n");
    
    // 2. Read current control word
    printf("2. Read current control word...\n");
    uint32_t current_control_word;
    if (read_sdo(sock, 0x6040, 0, &current_control_word) < 0) {
        printf("Read control word failed\n");
        return -1;
    }
    printf("  Current control word: 0x%04X\n", (uint16_t)current_control_word);
    
    // 3. Set control word bit4=1 (new position instruction)
    printf("3. Set control word bit4=1 (new position instruction)...\n");
    uint32_t new_control_word = current_control_word | 0x10; // Set bit4
    if (write_sdo(sock, 0x6040, 0, new_control_word) < 0) {
        printf("Set control word failed\n");
        return -1;
    }
    printf("    Control word set to: 0x%04X\n", (uint16_t)new_control_word);
    
    // 4. Wait for status word bit12=1 (instruction received)
    printf("4. Wait for status word bit12=1 (instruction received)...\n");
    int timeout_count = 0;
    uint32_t status_word;
    while (timeout_count < 50) { // 5 seconds timeout
        if (read_sdo(sock, 0x6041, 0, &status_word) == 0) {
            if (status_word & 0x1000) { // bit12=1
                printf("    Status word bit12=1, instruction received\n");
                break;
            }
        }
        usleep(100000); // 100ms
        timeout_count++;
        if (timeout_count % 10 == 0) {
            printf("   Waiting... (%d/50)\n", timeout_count);
        }
    }
    
    if (timeout_count >= 50) {
        printf("    Timeout: status word bit12 not changed to 1\n");
        return -1;
    }
    
    // 5. Set control word bit4=0 (release position instruction data)
    printf("5. Set control word bit4=0 (release position instruction data)...\n");
    uint32_t release_control_word = new_control_word & ~0x10; // Clear bit4
    if (write_sdo(sock, 0x6040, 0, release_control_word) < 0) {
        printf("Set control word failed\n");
        return -1;
    }
    printf("    Control word set to: 0x%04X\n", (uint16_t)release_control_word);
    
    // 6. Wait for status word bit12=0 (prepare to receive new instruction)
    printf("6. Wait for status word bit12=0 (prepare to receive new instruction)...\n");
    timeout_count = 0;
    while (timeout_count < 50) { // 5 seconds timeout
        if (read_sdo(sock, 0x6041, 0, &status_word) == 0) {
            if (!(status_word & 0x1000)) { // bit12=0
                printf("    Status word bit12=0, prepare to receive new instruction\n");
                break;
            }
        }
        usleep(100000); // 100ms timeout
        timeout_count++;
    }
    
    // 7. Check position change
    printf("7. Check position change...\n");
    usleep(1000000); // Wait 1 second
    
    uint32_t new_position;
    if (read_sdo(sock, 0x6064, 0, &new_position) == 0) {
        int32_t position_change = (int32_t)new_position - (int32_t)current_position;
        float change_turns = (float)position_change / MOTOR_RESOLUTION;
        printf("    New position: %d (%.2f turns)\n", (int32_t)new_position, (float)(int32_t)new_position / MOTOR_RESOLUTION);
        printf("    Position change: %d (%.3f turns)\n", position_change, change_turns);
        
        if (abs(position_change) < 100) { // Change less than 100 position units
            printf("    Warning: position change is too small, motor may not move!\n");
    } else {
            printf("    Motor movement normal!\n");
        }
    }
    
    printf("Position movement command executed\n");
    
    // Re-print command prompt
    print_command_help(sock);
    
    return 0;
}

/* Monitor motion status */
void monitor_motion(int sock) {
    uint32_t status_word;
    uint32_t actual_position;
    int loop_count = 0;
    
    printf("\n=== Monitor motion status ===\n");
    printf("Use keyboard commands to control the motor, input 'q' to exit the program\n\n");
    
    while (running) {
        loop_count++;
        
        // Read status word
        if (read_sdo(sock, 0x6041, 0, &status_word) == 0) {
            // Read actual position
            if (read_sdo(sock, 0x6064, 0, &actual_position) == 0) {
                // Print only when the status changes, to avoid screen flickering
                static uint32_t last_status = 0;
                static uint32_t last_position = 0;
                
                if (status_word != last_status || actual_position != last_position) {
                    printf("\n[Status] Status word: 0x%04X | Actual position: %d | Target position: %d", 
                           (uint16_t)status_word, (int32_t)actual_position, current_target_position);
            
            // Check if the target is reached
            if (status_word & 0x0400) {
                printf(" | Target reached!");
                    }
                    printf("\n");
                    fflush(stdout);
                    
                    last_status = status_word;
                    last_position = actual_position;
                }
            }
        }
        
        usleep(5000000); // 5 seconds update once, reduce frequency
    }
    
    printf("\nMonitor end\n");
}

/* Simplified node check - only send NMT start command */
int check_can_connection(int sock) {
    printf("=== Check CAN connection ===\n");
    struct can_frame frame;
    
    // Send NMT start command to all nodes
    frame.can_id = 0x000;
    frame.can_dlc = 2;
    frame.data[0] = 0x01;  // NMT start command
    frame.data[1] = 0x00;  // All nodes
    
    if (write(sock, &frame, sizeof(frame)) < 0) {
        printf("Send NMT start command failed\n");
        return -1;
    }
    
    usleep(500000);  // Wait 500ms for the node to start
    printf("CAN connection normal, start motor control\n");
    return 0;
}

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_can addr;
    struct ifreq ifr;
    const char *interface = "can0";
    
    signal(SIGINT, signal_handler);
    
    printf("eRob joint motor PP mode control program\n");
    printf("Mode: Profile Position Mode (PP Mode)\n");
    printf("Communication: CANopen SDO\n");
    printf("Based on: eRob CANopen and EtherCAT User Manual V1.9\n\n");
    
    // analyze command line parameters
    if (argc > 1) {
        int input_id = atoi(argv[1]);
        if (input_id >= 1 && input_id <= 127) {
            current_motor_id = input_id;
            printf("Using specified motor node ID: %d\n", current_motor_id);
        } else {
            printf("Error: node ID must be between 1 and 127\n");
            printf("Usage: %s [node ID]\n", argv[0]);
            printf("For example: %s 2\n", argv[0]);
            return 1;
        }
    } else {
        printf("Usage: %s [node ID]\n", argv[0]);
        printf("For example: %s 2\n", argv[0]);
        printf("Please specify motor node ID (1-127): ");
        
        int input_id;
        if (scanf("%d", &input_id) == 1) {
            if (input_id >= 1 && input_id <= 127) {
                current_motor_id = input_id;
                printf("Using motor node ID: %d\n", current_motor_id);
            } else {
                printf("Error: node ID must be between 1 and 127, using default value: %d\n", MOTOR_NODE_ID);
                current_motor_id = MOTOR_NODE_ID;
            }
        } else {
            printf("Invalid input, using default value: %d\n", MOTOR_NODE_ID);
            current_motor_id = MOTOR_NODE_ID;
        }
    }
    
    // Load EDS file
    if (parse_eds_file(EDS_FILE_PATH) < 0) {
        printf("EDS file load failed, using default data type\n");
    }
    
    // create CAN socket
    sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock < 0) {
        perror("Create socket failed");
        return 1;
    }
    
    // set CAN interface
    strcpy(ifr.ifr_name, interface);
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("Get interface index failed");
        close(sock);
        return 1;
    }
    
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind socket failed");
        close(sock);
        return 1;
    }
    
    // check CAN connection
    if (check_can_connection(sock) < 0) {
        printf("CAN connection check failed\n");
        close(sock);
        return 1;
    }
    
    // motor node ID is set in the program start
    
    // initialize PP mode
    if (init_pp_mode(sock) < 0) {
        printf("PP mode initialization failed\n");
        close(sock);
        return 1;
    }
    
    // ensure terminal settings are correct
    print_command_help(sock);
    
    // Main loop: keyboard input and status monitoring
    char input[256];
    while (running) {
        printf("\n>>> Enter command: ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) != NULL) {
            // Remove newline
            input[strcspn(input, "\n")] = 0;
            
            if (strlen(input) == 0) continue;
            
            char cmd = input[0];
            int parsed_value = 0;
            
            if (strlen(input) > 2) {
                sscanf(input + 2, "%d", &parsed_value);
            }
            
            switch (cmd) {
                case 'p': // Set target position
                    if (strlen(input) > 2) {
                        // There is a position parameter, execute position movement
                        current_target_position = parsed_value;
                        printf("Set target position: %d\n", current_target_position);
                        execute_position_move(sock, current_target_position);
                    } else {
                        // There is no position parameter, display current target position
                        printf("Current target position: %d\n", current_target_position);
                    }
                    break;
                    
                case 'v': // Set profile velocity
                    if (parsed_value > 0) {
                        current_profile_velocity = parsed_value;
                        printf("Set profile velocity: %d\n", current_profile_velocity);
                        write_sdo(sock, 0x6081, 0, current_profile_velocity);
                    } else {
                        printf("Current profile velocity: %d\n", current_profile_velocity);
                    }
                    print_command_help(sock);
                    break;
                    
                case 'a': // Set profile acceleration
                    if (parsed_value > 0) {
                        current_profile_acceleration = parsed_value;
                        printf("Set profile acceleration: %d\n", current_profile_acceleration);
                        write_sdo(sock, 0x6083, 0, current_profile_acceleration);
                    } else {
                        printf("Current profile acceleration: %d\n", current_profile_acceleration);
                    }
                    print_command_help(sock);
                    break;
                    
                case 'd': // Set profile deceleration
                    if (parsed_value > 0) {
                        current_profile_deceleration = parsed_value;
                        printf("Set profile deceleration: %d\n", current_profile_deceleration);
                        write_sdo(sock, 0x6084, 0, current_profile_deceleration);
                    } else {
                        printf("Current profile deceleration: %d\n", current_profile_deceleration);
                    }
                    print_command_help(sock);
                    break;
                    
                case '+': // Increase parameter
                    if (strlen(input) >= 2) {
                        switch (input[1]) {
                            case 'v':
                                current_profile_velocity += 100;
                                printf("Increase profile velocity to: %d\n", current_profile_velocity);
                                write_sdo(sock, 0x6081, 0, current_profile_velocity);
                                break;
                            case 'a':
                                current_profile_acceleration += 100;
                                printf("Increase profile acceleration to: %d\n", current_profile_acceleration);
                                write_sdo(sock, 0x6083, 0, current_profile_acceleration);
                                break;
                            case 'd':
                                current_profile_deceleration += 100;
                                    printf("Increase profile deceleration to: %d\n", current_profile_deceleration);
                                write_sdo(sock, 0x6084, 0, current_profile_deceleration);
                                break;
                        }
                    }
                    break;
                    
                case '-': // Decrease parameter
                    if (strlen(input) >= 2) {
                        switch (input[1]) {
                            case 'v':
                                if (current_profile_velocity > 100) {
                                    current_profile_velocity -= 100;
                                    printf("Decrease profile velocity to: %d\n", current_profile_velocity);
                                    write_sdo(sock, 0x6081, 0, current_profile_velocity);
                                } else {
                                    printf("Profile velocity cannot be less than 100\n");
                                }
                                break;
                            case 'a':
                                if (current_profile_acceleration > 100) {
                                    current_profile_acceleration -= 100;
                                    printf("Decrease profile acceleration to: %d\n", current_profile_acceleration);
                                    write_sdo(sock, 0x6083, 0, current_profile_acceleration);
                                } else {
                                    printf("Profile acceleration cannot be less than 100\n");
                                }
                                break;
                            case 'd':
                                if (current_profile_deceleration > 100) {
                                    current_profile_deceleration -= 100;
                                    printf("Decrease profile deceleration to: %d\n", current_profile_deceleration);
                                    write_sdo(sock, 0x6084, 0, current_profile_deceleration);
                                } else {
                                    printf("Profile deceleration cannot be less than 100\n");
                                }
                                break;
                        }
                    }
                    break;
                    
                case 's': // Stop motor
                    printf("Stop motor...\n");
                    write_sdo(sock, 0x6040, 0, 0x06); // Shutdown
                    print_command_help(sock);
                    break;
                    
                case 'q': // Exit program
                    printf("Exit program...\n");
                    running = 0;
                    break;
                    
                default:
                    printf("Unknown command: %c\n", cmd);
                    break;
            }
        }
        
        // Do not check status here, to avoid interfering with input
    }
    
    // 安全停止
    printf("\n\nStopping motor safely...\n");
    write_sdo(sock, 0x6040, 0, 0x06); // Shutdown
    usleep(200000);
    
    // Restore terminal settings
    restore_terminal();
    
    close(sock);
    printf("Program end\n");
    return 0;
}