  /*
 * CANopen电机扫描和详细信息读取程序
 * 支持快速扫描和详细读取两种模式
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
#include <signal.h>
#include <time.h>

#define QUICK_TIMEOUT_MS 100  // 100ms超时
#define DETAIL_TIMEOUT_MS 1000  // 1000ms超时
#define MAX_SCAN_NODES 20     // 只扫描前20个节点

volatile int running = 1;

void signal_handler(int sig) {
    running = 0;
    printf("\n程序被中断\n");
}

/* 发送SDO请求 */
int send_sdo_request(int sock, uint8_t node_id, uint16_t index, uint8_t subindex) {
    struct can_frame frame;
    frame.can_id = 0x600 + node_id;  // SDO Client to Server
    frame.can_dlc = 8;
    
    // SDO上传请求格式 (读取数据)
    frame.data[0] = 0x40;  // 上传请求
    frame.data[1] = index & 0xFF;
    frame.data[2] = (index >> 8) & 0xFF;
    frame.data[3] = subindex;
    frame.data[4] = 0;
    frame.data[5] = 0;
    frame.data[6] = 0;
    frame.data[7] = 0;
    
    return write(sock, &frame, sizeof(frame));
}

/* 接收SDO响应 */
int receive_sdo_response(int sock, uint8_t node_id, uint32_t *data, int timeout_ms) {
    struct can_frame frame;
    fd_set readfds;
    struct timeval timeout;
    int ret;
    clock_t start_time = clock();
    
    while ((clock() - start_time) * 1000 / CLOCKS_PER_SEC < timeout_ms) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000; // 10ms
        
        ret = select(sock + 1, &readfds, NULL, NULL, &timeout);
        if (ret > 0) {
            ret = read(sock, &frame, sizeof(frame));
            if (ret > 0) {
                // 检查是否是目标节点的SDO响应
                if (frame.can_id == (0x580 + node_id)) {
                    // 检查SDO响应格式
                    if ((frame.data[0] & 0xE0) == 0x40) {  // 上传响应
                        // 提取数据
                        *data = frame.data[4] | (frame.data[5] << 8) | 
                                (frame.data[6] << 16) | (frame.data[7] << 24);
                        return 0;
                    }
                }
            }
        }
    }
    
    return -1;  // 超时
}

/* 快速扫描单个节点 */
int quick_scan_node(int sock, uint8_t node_id) {
    uint32_t data;
    
    printf("节点 %d... ", node_id);
    fflush(stdout);
    
    // 只读取设备类型 (0x1000)
    if (send_sdo_request(sock, node_id, 0x1000, 0) < 0) {
        printf("发送失败\n");
        return -1;
    }
    
    if (receive_sdo_response(sock, node_id, &data, QUICK_TIMEOUT_MS) < 0) {
        printf("无响应\n");
        return -1;
    }
    
    // 检查是否是CiA402设备 (0x92 或 0x0192 = 电机驱动器)
    uint16_t device_type = data & 0xFFFF;
    if (device_type != 0x92 && device_type != 0x0192) {
        printf("非电机 (0x%04X)\n", device_type);
        return -1;
    }
    
    printf("✓ 电机设备! (0x%08X)\n", data);
    return 0;
}

/* 读取节点详细信息 */
int read_node_info(int sock, uint8_t node_id) {
    uint32_t data;
    
    printf("=== 节点 %d 详细信息 ===\n", node_id);
    
    // 读取设备类型 (0x1000)
    printf("读取设备类型 (0x1000)... ");
    if (send_sdo_request(sock, node_id, 0x1000, 0) < 0) {
        printf("发送失败\n");
        return -1;
    }
    
    if (receive_sdo_response(sock, node_id, &data, DETAIL_TIMEOUT_MS) < 0) {
        printf("无响应\n");
        return -1;
    }
    
    printf("0x%08X\n", data);
    printf("  设备类型: 0x%04X\n", data & 0xFFFF);
    printf("  厂商特定: %s\n", (data & 0xFFFF) == 0x92 ? "标准CiA402" : "厂商特定");
    
    // 读取错误寄存器 (0x1001)
    printf("读取错误寄存器 (0x1001)... ");
    if (send_sdo_request(sock, node_id, 0x1001, 0) >= 0) {
        if (receive_sdo_response(sock, node_id, &data, DETAIL_TIMEOUT_MS) >= 0) {
            printf("0x%02X\n", data & 0xFF);
            printf("  错误状态: %s\n", (data & 0xFF) == 0 ? "正常" : "有错误");
        } else {
            printf("无响应\n");
        }
    }
    
    // 读取厂商ID (0x1018:1)
    printf("读取厂商ID (0x1018:1)... ");
    if (send_sdo_request(sock, node_id, 0x1018, 1) >= 0) {
        if (receive_sdo_response(sock, node_id, &data, DETAIL_TIMEOUT_MS) >= 0) {
            printf("0x%08X\n", data);
            printf("  厂商: ");
            switch (data) {
                case 0x5A65726F:  // ZEROERR CONTROL
                    printf("ZeroErr Control");
                    break;
                case 0x00000001:
                    printf("示例厂商");
                    break;
                default:
                    printf("未知厂商");
                    break;
            }
            printf("\n");
        } else {
            printf("无响应\n");
        }
    }
    
    // 读取产品代码 (0x1018:2)
    printf("读取产品代码 (0x1018:2)... ");
    if (send_sdo_request(sock, node_id, 0x1018, 2) >= 0) {
        if (receive_sdo_response(sock, node_id, &data, DETAIL_TIMEOUT_MS) >= 0) {
            printf("0x%08X\n", data);
        } else {
            printf("无响应\n");
        }
    }
    
    // 读取版本号 (0x1018:3)
    printf("读取版本号 (0x1018:3)... ");
    if (send_sdo_request(sock, node_id, 0x1018, 3) >= 0) {
        if (receive_sdo_response(sock, node_id, &data, DETAIL_TIMEOUT_MS) >= 0) {
            printf("0x%08X\n", data);
        } else {
            printf("无响应\n");
        }
    }
    
    // 读取序列号 (0x1018:4)
    printf("读取序列号 (0x1018:4)... ");
    if (send_sdo_request(sock, node_id, 0x1018, 4) >= 0) {
        if (receive_sdo_response(sock, node_id, &data, DETAIL_TIMEOUT_MS) >= 0) {
            printf("0x%08X\n", data);
        } else {
            printf("无响应\n");
        }
    }
    
    // 尝试读取CiA402相关对象
    printf("\n=== CiA402 电机控制对象 ===\n");
    
    // 读取控制字 (0x6040)
    printf("读取控制字 (0x6040)... ");
    if (send_sdo_request(sock, node_id, 0x6040, 0) >= 0) {
        if (receive_sdo_response(sock, node_id, &data, DETAIL_TIMEOUT_MS) >= 0) {
            printf("0x%04X\n", data & 0xFFFF);
        } else {
            printf("无响应\n");
        }
    }
    
    // 读取状态字 (0x6041)
    printf("读取状态字 (0x6041)... ");
    if (send_sdo_request(sock, node_id, 0x6041, 0) >= 0) {
        if (receive_sdo_response(sock, node_id, &data, DETAIL_TIMEOUT_MS) >= 0) {
            printf("0x%04X\n", data & 0xFFFF);
            printf("  状态: ");
            uint16_t status = data & 0xFFFF;
            if (status & 0x0001) printf("准备就绪 ");
            if (status & 0x0002) printf("已切换 ");
            if (status & 0x0004) printf("操作使能 ");
            if (status & 0x0008) printf("故障 ");
            if (status & 0x0010) printf("电压使能 ");
            if (status & 0x0020) printf("快速停止 ");
            if (status & 0x0040) printf("开关禁用 ");
            if (status & 0x0080) printf("警告 ");
            if (status & 0x0100) printf("制造商特定 ");
            if (status & 0x0200) printf("远程 ");
            if (status & 0x0400) printf("目标达到 ");
            if (status & 0x0800) printf("内部限制 ");
            printf("\n");
        } else {
            printf("无响应\n");
        }
    }
    
    // 读取操作模式 (0x6060)
    printf("读取操作模式 (0x6060)... ");
    if (send_sdo_request(sock, node_id, 0x6060, 0) >= 0) {
        if (receive_sdo_response(sock, node_id, &data, DETAIL_TIMEOUT_MS) >= 0) {
            printf("%d\n", data & 0xFF);
            printf("  模式: ");
            switch (data & 0xFF) {
                case 0: printf("无模式"); break;
                case 1: printf("位置模式"); break;
                case 2: printf("速度模式"); break;
                case 3: printf("速度轮廓模式"); break;
                case 4: printf("扭矩模式"); break;
                case 6: printf("回零模式"); break;
                case 7: printf("插补位置模式"); break;
                case 8: printf("循环同步位置模式"); break;
                case 9: printf("循环同步速度模式"); break;
                case 10: printf("循环同步扭矩模式"); break;
                default: printf("未知模式"); break;
            }
            printf("\n");
        } else {
            printf("无响应\n");
        }
    }
    
    printf("\n=== 结论 ===\n");
    printf("节点 %d 是一个CANopen设备，", node_id);
    if ((data & 0xFFFF) == 0x92 || (data & 0xFFFF) == 0x0192) {
        printf("很可能是电机驱动器！\n");
        printf("建议使用节点 %d 进行电机控制。\n", node_id);
    } else {
        printf("但不是标准的CiA402电机驱动器。\n");
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_can addr;
    struct ifreq ifr;
    const char *interface = "can0";
    int found_count = 0;
    int max_nodes = MAX_SCAN_NODES;
    int mode = 0; // 0=扫描模式, 1=详细读取模式
    uint8_t target_node = 2;
    
    // 解析命令行参数
    if (argc > 1) {
        if (strcmp(argv[1], "read") == 0 || strcmp(argv[1], "detail") == 0) {
            mode = 1; // 详细读取模式
            if (argc > 2) {
                target_node = atoi(argv[2]);
            }
        } else {
            interface = argv[1];
        }
    }
    if (argc > 2 && mode == 0) {
        max_nodes = atoi(argv[2]);
        if (max_nodes > 127) max_nodes = 127;
    }
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    
    if (mode == 1) {
        printf("CANopen设备详细信息读取工具\n");
        printf("接口: %s\n", interface);
        printf("目标节点: %d\n\n", target_node);
    } else {
        printf("快速CANopen电机扫描工具\n");
        printf("接口: %s\n", interface);
        printf("扫描范围: 节点1-%d\n", max_nodes);
        printf("超时: %dms\n", QUICK_TIMEOUT_MS);
        printf("按Ctrl+C停止扫描\n\n");
        printf("使用方法:\n");
        printf("  %s                    # 快速扫描模式\n", argv[0]);
        printf("  %s read [节点ID]      # 详细读取模式\n", argv[0]);
        printf("  %s can0 50            # 扫描can0接口，节点1-50\n\n", argv[0]);
    }
    
    // 创建CAN socket
    sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock < 0) {
        perror("创建socket失败");
        return 1;
    }
    
    // 设置CAN接口
    strcpy(ifr.ifr_name, interface);
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("获取接口索引失败");
        close(sock);
        return 1;
    }
    
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("绑定socket失败");
        close(sock);
        return 1;
    }
    
    if (mode == 1) {
        // 详细读取模式
        read_node_info(sock, target_node);
    } else {
        // 快速扫描模式
        printf("开始快速扫描...\n");
        
        // 快速扫描节点
        for (int node_id = 1; node_id <= max_nodes && running; node_id++) {
            if (quick_scan_node(sock, node_id) == 0) {
                found_count++;
            }
            usleep(10000);  // 10ms延迟
        }
        
        printf("\n扫描完成!\n");
        printf("找到 %d 个电机设备\n", found_count);
        
        if (found_count == 0) {
            printf("\n未找到电机设备。可能的原因:\n");
            printf("1. 电机未上电\n");
            printf("2. 电机节点ID不在1-%d范围内\n", max_nodes);
            printf("3. 电机波特率不是1Mbps\n");
            printf("4. CAN连接问题\n");
            printf("\n建议:\n");
            printf("- 尝试扫描更多节点: ./quick_scan can0 50\n");
            printf("- 检查电机配置\n");
            printf("- 使用CAN分析仪监控CAN总线\n");
        } else {
            printf("\n要查看某个节点的详细信息，请使用:\n");
            printf("  ./quick_scan read [节点ID]\n");
        }
    }
    
    close(sock);
    return 0;
}
