# CANopenNode Examples
# 这个文件是作为子目录被上级CMakeLists.txt调用的

# 设置包含目录
include_directories(
    .
    ..
    301
    303
    304
    305
    309
    extra
    storage
)

# 1. 原始示例程序 (canopennode_blank)
add_executable(canopennode_blank
    main_blank.c
    CO_driver_blank.c
    CO_storageBlank.c
    OD.c
    ../CANopen.c
)

target_link_libraries(canopennode_blank canopennode)

# 2. 快速扫描程序 (quick_scan)
add_executable(quick_scan
    quick_scan.c
)

target_link_libraries(quick_scan canopennode)

# 3. PP模式控制程序 (pp_mode_control)
add_executable(pp_mode_control
    pp_mode_control.c
)

target_link_libraries(pp_mode_control canopennode)

# 设置输出目录
set_target_properties(canopennode_blank PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(quick_scan PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(pp_mode_control PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 安装规则
install(TARGETS canopennode_blank quick_scan pp_mode_control
    RUNTIME DESTINATION bin
)

# 安装配置文件
install(FILES
    "ZeroErr Driver_V1.5.eds"
    "DS301_profile.xpd"
    "README_pp_mode_control.md"
    DESTINATION share/canopennode
)

# 创建自定义目标
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E remove -f *.o
    COMMAND ${CMAKE_COMMAND} -E remove -f canopennode_blank
    COMMAND ${CMAKE_COMMAND} -E remove -f quick_scan
    COMMAND ${CMAKE_COMMAND} -E remove -f pp_mode_control
    COMMENT "Cleaning all build files"
)

# 打印配置信息
message(STATUS "CANopenNode Examples Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  C flags: ${CMAKE_C_FLAGS}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Available targets:")
message(STATUS "  canopennode_blank  - Original CANopenNode example")
message(STATUS "  quick_scan         - CANopen device scanner")
message(STATUS "  pp_mode_control    - CiA402 PP mode controller")
message(STATUS "  clean-all          - Clean all build files")
message(STATUS "")
message(STATUS "Usage:")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake ..")
message(STATUS "  make")
message(STATUS "  ./bin/quick_scan")
message(STATUS "  ./bin/quick_scan read 2")
message(STATUS "  ./bin/pp_mode_control")