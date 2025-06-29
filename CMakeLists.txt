# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================



cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)
# Define o nome do projeto como Teste_ldr, suportando C, C++ e Assembly.
project(Teste_ldr C CXX ASM) 
pico_sdk_init()
add_executable(${PROJECT_NAME}  
        main.c  # Código principal em C para o LDR
        lib/ssd1306.c # Biblioteca para o display OLED
        lib/webserver.c
        )

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/lib)
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/lib)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
        PICO_PRINTF_SUPPORT_FLOAT=1 
        PICO_STDIO_ENABLE_PRINTF=1
    )
target_link_libraries(${PROJECT_NAME} 
        pico_stdlib 
        hardware_i2c
        hardware_adc
        hardware_pwm
        hardware_pio
        pico_cyw43_arch_lwip_threadsafe_background
        )

target_include_directories(Teste_ldr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME})


