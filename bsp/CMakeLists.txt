set(BSP_GENERATOR
    ${CMAKE_SOURCE_DIR}/tools/bsp_generator/nm_pinconfig.py
)

set(BSP_PINS_SRC ${CMAKE_CURRENT_LIST_DIR}/${BSP_TARGET_DIR}/bsp_pins.src)
set(BSP_PINS_H ${CMAKE_CURRENT_BINARY_DIR}/am_bsp_pins.h)
set(BSP_PINS_C ${CMAKE_CURRENT_BINARY_DIR}/am_bsp_pins.c)

set(BSP_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    CACHE PATH "BSP includes"
)

add_custom_command(
    OUTPUT
        ${BSP_PINS_H}
        ${BSP_PINS_C}
    COMMAND
        python ${BSP_GENERATOR} ${BSP_PINS_SRC} ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
        ${BSP_PINS_SRC}
)

add_library(
    bsp
    STATIC
)

target_include_directories(
    bsp
    PRIVATE
    ${HAL_INCLUDES}
    ${BSP_INCLUDES}
)

target_sources(
    bsp PRIVATE
    am_bsp.c
    ${BSP_PINS_C}
    ${BSP_PINS_H}
)