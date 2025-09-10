# VSCode 配置说明

## 构建系统

CANopenNode 现在使用 CMake 作为构建系统。所有编译产物都统一放在上级目录的 `build` 文件夹中。

## VSCode 任务

### 编译任务
- **build-all**: 编译所有目标（库和示例程序）
  - 使用 `Ctrl+Shift+P` → `Tasks: Run Task` → `build-all`
  - 或者使用快捷键 `Ctrl+Shift+B` (如果设置为默认构建任务)

- **build-examples**: 只编译示例程序
  - 使用 `Ctrl+Shift+P` → `Tasks: Run Task` → `build-examples`

- **clean-build**: 清理构建文件
  - 使用 `Ctrl+Shift+P` → `Tasks: Run Task` → `clean-build`

### 运行任务
- **run-canopennode-blank**: 运行基础示例程序
- **run-quick-scan**: 运行设备扫描工具
- **run-pp-mode-control**: 运行PP模式控制程序 (需要sudo权限)

## 调试配置

### 调试选项
1. **Debug CANopenNode Blank**: 调试基础示例程序
   - 程序路径: `${workspaceFolder}/../build/bin/canopennode_blank`
   - 预启动任务: `build-all`

2. **Debug Quick Scan**: 调试设备扫描工具
   - 程序路径: `${workspaceFolder}/../build/bin/quick_scan`
   - 预启动任务: `build-all`

3. **Debug PP Mode Control**: 调试PP模式控制程序
   - 程序路径: `${workspaceFolder}/../build/bin/pp_mode_control`
   - 预启动任务: `build-all`

4. **Debug PP Mode Control (with sudo)**: 带sudo权限的调试模式
   - 程序路径: `sudo`
   - 参数: `["./build/bin/pp_mode_control"]`
   - 预启动任务: `build-all`

## 使用方法

### 1. 编译程序
```bash
# 方法1: 使用VSCode任务
Ctrl+Shift+P → Tasks: Run Task → build-all

# 方法2: 使用终端
cd .. && mkdir -p build && cd build
cmake .. && make
```

### 2. 运行程序
```bash
# 方法1: 使用VSCode任务
Ctrl+Shift+P → Tasks: Run Task → run-[program-name]

# 方法2: 使用终端
cd ../build
./bin/canopennode_blank    # 基础示例
./bin/quick_scan           # 设备扫描
sudo ./bin/pp_mode_control # PP模式控制
```

### 3. 调试程序
1. 在代码中设置断点
2. 按 `F5` 或点击调试按钮
3. 选择相应的调试配置

## 注意事项

- 所有编译产物都在 `../build/` 目录中
- PP模式控制程序需要sudo权限访问CAN接口
- 确保CAN接口已正确配置 (can0)
- EDS文件 `ZeroErr Driver_V1.5.eds` 必须在example目录下
- 电机节点ID默认为2，可在代码中修改 `MOTOR_NODE_ID` 常量

## 文件结构

```
CANopenNode/
├── build/                    # 编译输出目录
│   ├── bin/                 # 可执行文件
│   │   ├── canopennode_blank
│   │   ├── quick_scan
│   │   └── pp_mode_control
│   └── libcanopennode.a     # 静态库
├── example/                 # 示例程序目录
│   ├── pp_mode_control.c    # PP模式控制程序
│   ├── quick_scan.c         # 设备扫描工具
│   ├── main_blank.c         # 基础示例程序
│   ├── ZeroErr Driver_V1.5.eds  # EDS文件
│   └── .vscode/             # VSCode配置
└── 301/, 303/, 304/, ...    # CANopenNode核心模块
```

## 快速开始

1. 打开VSCode
2. 打开CANopenNode项目根目录
3. 按 `Ctrl+Shift+P` → `Tasks: Run Task` → `build-all`
4. 等待编译完成
5. 按 `F5` 开始调试或使用运行任务
