# set(COMPONENT_SRCS "intercom_audio.c" "app_main.c  gui.c sime.c wifi.c")
# set(COMPONENT_ADD_INCLUDEDIRS "")
#
# register_component()

idf_component_register(
    SRCS "app_main.c" "gui.c" "sime.c" "wifi.c" "https_server.c" "keep_alive.c" "intercom_audio.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "servercert.pem" "prvtkey.pem"
)
