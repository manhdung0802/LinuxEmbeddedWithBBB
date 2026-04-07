cmake_minimum_required(VERSION 3.16)
project(bbb_simulator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find GTK3
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

# Find Unicorn Engine
find_library(UNICORN_LIB unicorn REQUIRED)
find_path(UNICORN_INCLUDE unicorn/unicorn.h)

# Find pthreads
find_package(Threads REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${GTK3_INCLUDE_DIRS}
    ${UNICORN_INCLUDE}
)

link_directories(${GTK3_LIBRARY_DIRS})
add_definitions(${GTK3_CFLAGS_OTHER})

# Compiler warnings
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# Source files
set(PERIPH_SOURCES
    src/periph/gpio.c
    src/periph/prcm.c
    src/periph/control.c
    src/periph/uart.c
    src/periph/i2c.c
    src/periph/spi.c
    src/periph/timer.c
    src/periph/adc.c
    src/periph/intc.c
)

set(GUI_SOURCES
    src/gui/gui_main.c
    src/gui/gui_gpio.c
    src/gui/gui_uart.c
    src/gui/gui_registers.c
    src/gui/gui_memory.c
)

set(CORE_SOURCES
    src/main.c
    src/bbb_sim.c
    src/cpu_emu.c
    src/mem_subsys.c
    src/elf_loader.c
    src/ko_loader.c
    src/syscall_emu.c
)

add_executable(bbb_simulator
    ${CORE_SOURCES}
    ${PERIPH_SOURCES}
    ${GUI_SOURCES}
)

target_link_libraries(bbb_simulator
    ${GTK3_LIBRARIES}
    ${UNICORN_LIB}
    Threads::Threads
    m
)

install(TARGETS bbb_simulator DESTINATION bin)
