cmake_minimum_required(VERSION 3.14)
project(os101_lock_lab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 编译选项
add_compile_options(-Wall -Wextra -g)

# 头文件目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 线程库
find_package(Threads REQUIRED)

# ============ 测试可执行文件 ============

# 测试 my::atomic
add_executable(test_atomic tests/test_atomic.cpp)
target_link_libraries(test_atomic Threads::Threads)

# 测试 my::spin_lock
add_executable(test_spin_lock tests/test_spin_lock.cpp)
target_link_libraries(test_spin_lock Threads::Threads)

# 测试 my::semaphore
add_executable(test_semaphore tests/test_semaphore.cpp)
target_link_libraries(test_semaphore Threads::Threads)

# 测试 Ticket Spin Lock
add_executable(test_ticket_spin_lock tests/test_ticket_spin_lock.cpp)
target_link_libraries(test_ticket_spin_lock Threads::Threads)

# 测试 MCS Lock
add_executable(test_mcs_lock tests/test_mcs_lock.cpp)
target_link_libraries(test_mcs_lock Threads::Threads)

# 锁性能对比测试
add_executable(benchmark_locks tests/benchmark_locks.cpp)
target_link_libraries(benchmark_locks Threads::Threads)
target_compile_options(benchmark_locks PRIVATE -O2)  # 开启优化以获得更准确的性能数据

# ============ 应用示例 ============

# 生产者-消费者模型
add_executable(producer_consumer examples/producer_consumer.cpp)
target_link_libraries(producer_consumer Threads::Threads)

# 哲学家进餐问题
add_executable(dining_philosophers examples/dining_philosophers.cpp)
target_link_libraries(dining_philosophers Threads::Threads)

# ============ 便捷目标 ============
add_custom_target(run_all_tests
    COMMAND ${CMAKE_BINARY_DIR}/test_atomic
    COMMAND ${CMAKE_BINARY_DIR}/test_spin_lock
    COMMAND ${CMAKE_BINARY_DIR}/test_semaphore
    COMMAND ${CMAKE_BINARY_DIR}/test_ticket_spin_lock
    COMMAND ${CMAKE_BINARY_DIR}/test_mcs_lock
    DEPENDS test_atomic test_spin_lock test_semaphore test_ticket_spin_lock test_mcs_lock
    COMMENT "Running all tests..."
)
