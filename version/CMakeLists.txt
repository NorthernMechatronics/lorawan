set(NM_APP_VERSION_H ${CMAKE_CURRENT_LIST_DIR}/nm_app_version.h CACHE PATH "")
set(NM_APP_VERSION_C ${CMAKE_CURRENT_LIST_DIR}/nm_app_version.c CACHE PATH "")

add_library(
    nm_app_version
    STATIC
)

set (NM_APP_VERSION_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}
    CACHE PATH "APP version includes"
)

target_include_directories(
    nm_app_version
    PRIVATE
    ${NM_APP_VERSION_INCLUDES}
)

target_sources(
    nm_app_version
    PRIVATE
    ${NM_APP_VERSION_C}
)
