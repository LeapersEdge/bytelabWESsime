set(SQUARELINE_DIR ${CMAKE_CURRENT_LIST_DIR}/squareline/project)
file(GLOB_RECURSE SQUARELINE_SRCS
    "${SQUARELINE_DIR}/*.c"
)

set(COMPONENT_SRCS 
    "ui_app.c app_state.c sime.c wifi.c rowing_game.c audio.c wifi.c https_server.c keep_alive.c intercom_audio.c parental_lock.c"
    ${SQUARELINE_SRCS}
)

file(GLOB_RECURSE SQUARELINE_INCLUDES
    "${SQUARELINE_DIR}/*.h"
)

foreach(_hdr ${SQUARELINE_INCLUDES})
    get_filename_component(_dir ${_hdr} DIRECTORY)
    list(APPEND COMPONENT_ADD_INCLUDEDIRS ${_dir})
endforeach()

list(REMOVE_DUPLICATES COMPONENT_ADD_INCLUDEDIRS)

set(COMPONENT_EMBED_TXTFILES
    "servercert.pem"
    "prvtkey.pem"
)

set(COMPONENT_PRIV_REQUIRES lvgl lvgl_esp32_drivers esp_wifi nvs_flash freertos esp_timer esp_driver_gpio esp_driver_ledc esp_https_server)

register_component()

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=unused-value)
