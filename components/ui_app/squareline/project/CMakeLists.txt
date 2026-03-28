SET(SOURCES screens/ui_SimeAppsScr.c
    screens/ui_CallScr.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_background_png.c
    images/ui_img_sime_dirty_lines_png.c
    images/ui_img_sime_happy_png.c
    images/ui_img_sime_neutral_png.c
    images/ui_img_sime_sad_png.c
    images/ui_img_sime_dying_png.c
    images/ui_img_unselected_camera_png.c
    images/ui_img_icon_camera_png.c
    images/ui_img_unselected_album_png.c
    images/ui_img_icon_album_png.c
    images/ui_img_unselected_music_png.c
    images/ui_img_icon_music_png.c
    images/ui_img_unselected_phone_png.c
    images/ui_img_icon_phone_png.c
    images/ui_img_unselected_cleaning_png.c
    images/ui_img_icon_cleaning_png.c
    images/ui_img_unselected_food_png.c
    images/ui_img_icon_food_png.c
    images/ui_img_unselected_hangup_png.c
    images/ui_img_icon_hangup_png.c)

add_library(ui ${SOURCES})
