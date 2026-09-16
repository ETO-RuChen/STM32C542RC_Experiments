# file-format: 1.0.0
cmake_minimum_required(VERSION 3.30)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

include(cmake/target.cmake)
include(cmake/${TOOLCHAIN_FILE})

project(05_DMA_List)
add_executable(05_DMA_List)
include(cmake/files.cmake)
include(cmake/flags.cmake)
include(cmake/components.cmake)

# Stubs to be able to add compiler flag overrides, e.g. -Os
set(APP_PHASE "6" CACHE STRING "Bring-up phase: 1=fixed PWM, 2=direct, 3=timer list, 4=mixed, 5=normal, 6=alarm")
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  App/app_main.c
  App/bringup_dma.c
  Drivers_App/dma_graph.c
  Drivers_App/pwm_waveform.c
  Drivers_App/logger_dma.c
  BSP/bsp_led.c
  BSP/bsp_button.c
  BSP/bsp_vcp.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
  App
  BSP
  Config
  Drivers_App
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC
  APP_PHASE=${APP_PHASE}
)

target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC
  "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/Config/app_hal_overrides.h"
)

target_link_options(${CMAKE_PROJECT_NAME} PUBLIC
  # Add additional linker flags here
)

target_link_directories(${CMAKE_PROJECT_NAME} PUBLIC
  # Add additional linker search directories here
)

target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC
  # Add additional linker libraries here
)
