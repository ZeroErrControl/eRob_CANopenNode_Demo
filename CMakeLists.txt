cmake_minimum_required(VERSION 3.10)
project(CANopenNode)

# 设置C标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g -O2")

# 设置包含目录
include_directories(
    .
    301
    303
    304
    305
    309
    extra
    storage
)

# 定义CANopenNode源文件
set(CANOPEN_SOURCES
    301/CO_Emergency.c
    301/CO_fifo.c
    301/CO_HBconsumer.c
    301/CO_NMT_Heartbeat.c
    301/CO_Node_Guarding.c
    301/CO_ODinterface.c
    301/CO_PDO.c
    301/CO_SDOclient.c
    301/CO_SDOserver.c
    301/CO_SYNC.c
    301/CO_TIME.c
    301/crc16-ccitt.c
    303/CO_LEDs.c
    304/CO_GFC.c
    304/CO_SRDO.c
    305/CO_LSSmaster.c
    305/CO_LSSslave.c
    309/CO_gateway_ascii.c
    extra/CO_trace.c
    storage/CO_storage.c
)

# 定义CANopenNode头文件
set(CANOPEN_HEADERS
    301/CO_config.h
    301/CO_driver.h
    301/CO_Emergency.h
    301/CO_fifo.h
    301/CO_HBconsumer.h
    301/CO_NMT_Heartbeat.h
    301/CO_Node_Guarding.h
    301/CO_ODinterface.h
    301/CO_PDO.h
    301/CO_SDOclient.h
    301/CO_SDOserver.h
    301/CO_SYNC.h
    301/CO_TIME.h
    301/crc16-ccitt.h
    303/CO_LEDs.h
    304/CO_GFC.h
    304/CO_SRDO.h
    305/CO_LSS.h
    305/CO_LSSmaster.h
    305/CO_LSSslave.h
    309/CO_gateway_ascii.h
    extra/CO_trace.h
    storage/CO_eeprom.h
    storage/CO_storage.h
    storage/CO_storageEeprom.h
    CANopen.h
)

# 创建CANopenNode静态库
add_library(canopennode STATIC ${CANOPEN_SOURCES} ${CANOPEN_HEADERS})

# 设置库的包含目录
target_include_directories(canopennode PUBLIC
    .
    301
    303
    304
    305
    309
    extra
    storage
)

# 添加example子目录
add_subdirectory(example)

# 安装规则
install(TARGETS canopennode
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 安装头文件
install(DIRECTORY 301/ 303/ 304/ 305/ 309/ extra/ storage/
    DESTINATION include/canopennode
    FILES_MATCHING PATTERN "*.h"
)

install(FILES CANopen.h
    DESTINATION include/canopennode
)

# 打印配置信息
message(STATUS "CANopenNode Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  C flags: ${CMAKE_C_FLAGS}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Available targets:")
message(STATUS "  canopennode         - CANopenNode static library")
message(STATUS "  canopennode_blank   - Original CANopenNode example")
message(STATUS "  quick_scan          - CANopen device scanner")
message(STATUS "  pp_mode_control     - CiA402 PP mode controller")
message(STATUS "")
message(STATUS "Usage:")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake ..")
message(STATUS "  make")
message(STATUS "  ./bin/quick_scan")
message(STATUS "  ./bin/quick_scan read 2")
message(STATUS "  ./bin/pp_mode_control")
