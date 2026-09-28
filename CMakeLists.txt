cmake_minimum_required(VERSION 3.15)
project(MicroRTOS_Scheduler C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(include)

# Kernel Core Source Files
set(KERNEL_SRCS
    src/kernel/rtos_kernel.c
    src/kernel/rtos_task.c
    src/kernel/rtos_queue.c
    src/kernel/rtos_sync.c
    src/kernel/rtos_timer.c
    src/kernel/rtos_trace.c
)

# Platform Port
if(WIN32)
    list(APPEND KERNEL_SRCS src/port/port_win32.c)
    set(PORT_LIBS winmm)
else()
    list(APPEND KERNEL_SRCS src/port/port_posix.c)
    set(PORT_LIBS pthread m)
endif()

# Build Static Kernel Library
add_library(micro_rtos STATIC ${KERNEL_SRCS})
target_link_libraries(micro_rtos PUBLIC ${PORT_LIBS})

# Build Executable Demos
add_executable(01_cooperative_demo examples/01_cooperative_demo.c)
target_link_libraries(01_cooperative_demo PRIVATE micro_rtos)

add_executable(02_preemptive_priority_demo examples/02_preemptive_priority_demo.c)
target_link_libraries(02_preemptive_priority_demo PRIVATE micro_rtos)

add_executable(03_priority_inversion_pip_demo examples/03_priority_inversion_pip_demo.c)
target_link_libraries(03_priority_inversion_pip_demo PRIVATE micro_rtos)

add_executable(04_producer_consumer_queue_demo examples/04_producer_consumer_queue_demo.c)
target_link_libraries(04_producer_consumer_queue_demo PRIVATE micro_rtos)

add_executable(05_flight_telemetry_sim examples/05_flight_telemetry_sim.c)
target_link_libraries(05_flight_telemetry_sim PRIVATE micro_rtos)

# Build Test Suite
add_executable(test_suite tests/test_suite.c)
target_link_libraries(test_suite PRIVATE micro_rtos)
