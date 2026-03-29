#include "./app_state.h"
#include "./rowing_game.h"
#include "./audio.h"
#include "ui_app.h"
#include "./squareline/project/ui.h"

/*
 * Reset-default state:
 * Since these are static globals, after reset they will be initialized
 * back to these values.
 */
static volatile app_state_t g_app_state = APP_STATE_HOME_SCREEN;
static volatile app_state_t g_previous_app_state = APP_STATE_HOME_SCREEN;

static volatile int _curr_song = 5;

static bool is_valid_state(app_state_t state);
static void exit_state(app_state_t state);
static void enter_state(app_state_t state);

/* Per-state entry functions */
static void enter_home_screen(void);
static void enter_turn_off(void);
static void enter_camera(void);
static void enter_gallery(void);
static void enter_contacts(void);
static void enter_call_screen_1(void);
static void enter_call_screen_2(void);
static void enter_call_screen_3(void);
static void enter_call_screen_4(void);
static void enter_intercom(void);
static void enter_music(void);
static void enter_song_1(void);
static void enter_song_2(void);
static void enter_song_3(void);
static void enter_feeding_camera(void);
static void enter_math(void);
static void enter_weslanje(void);
static void enter_parental_lock(void);

/* Optional per-state exit functions */
static void exit_home_screen(void);
static void exit_turn_off(void);
static void exit_camera(void);
static void exit_gallery(void);
static void exit_contacts(void);
static void exit_call_screen_1(void);
static void exit_call_screen_2(void);
static void exit_call_screen_3(void);
static void exit_call_screen_4(void);
static void exit_intercom(void);
static void exit_music(void);
static void exit_song_1(void);
static void exit_song_2(void);
static void exit_song_3(void);
static void exit_feeding_camera(void);
static void exit_math(void);
static void exit_weslanje(void);
static void exit_parental_lock(void);

bool set_app_state(app_state_t new_state)
{
    app_state_t old_state;

    if (!is_valid_state(new_state))
    {
        return false;
    }

    old_state = g_app_state;

    if (old_state == new_state)
    {
        return true;
    }

    /*
     * Blocking logical transition:
     * 1. Exit old state
     * 2. Save previous state
     * 3. Update current state
     * 4. Enter new state
     */
    exit_state(old_state);

    g_previous_app_state = old_state;
    g_app_state = new_state;

    enter_state(new_state);

    return true;
}

app_state_t get_app_state(void)
{
    return g_app_state;
}

app_state_t get_previous_app_state(void)
{
    return g_previous_app_state;
}

static bool is_valid_state(app_state_t state)
{
    switch (state)
    {
        case APP_STATE_HOME_SCREEN:
        case APP_STATE_TURN_OFF:
        case APP_STATE_CAMERA:
        case APP_STATE_GALLERY:
        case APP_STATE_CONTACTS:
        case APP_STATE_CALL_SCREEN_1:
        case APP_STATE_CALL_SCREEN_2:
        case APP_STATE_CALL_SCREEN_3:
        case APP_STATE_CALL_SCREEN_4:
        case APP_STATE_INTERCOM:
        case APP_STATE_MUSIC:
        case APP_STATE_SONG_1:
        case APP_STATE_SONG_2:
        case APP_STATE_SONG_3:
        case APP_STATE_FEEDING_CAMERA:
        case APP_STATE_MATH:
        case APP_STATE_WESLANJE:
        case APP_STATE_PARENTAL_LOCK:
            return true;

        default:
            return false;
    }
}

static void exit_state(app_state_t state)
{
    switch (state)
    {
        case APP_STATE_HOME_SCREEN:
            exit_home_screen();
            break;

        case APP_STATE_TURN_OFF:
            exit_turn_off();
            break;

        case APP_STATE_CAMERA:
            exit_camera();
            break;

        case APP_STATE_GALLERY:
            exit_gallery();
            break;

        case APP_STATE_CONTACTS:
            exit_contacts();
            break;

        case APP_STATE_CALL_SCREEN_1:
            exit_call_screen_1();
            break;

        case APP_STATE_CALL_SCREEN_2:
            exit_call_screen_2();
            break;

        case APP_STATE_CALL_SCREEN_3:
            exit_call_screen_3();
            break;

        case APP_STATE_CALL_SCREEN_4:
            exit_call_screen_4();
            break;

        case APP_STATE_INTERCOM:
            exit_intercom();
            break;

        case APP_STATE_MUSIC:
            exit_music();
            break;

        case APP_STATE_SONG_1:
            exit_song_1();
            break;

        case APP_STATE_SONG_2:
            exit_song_2();
            break;

        case APP_STATE_SONG_3:
            exit_song_3();
            break;

        case APP_STATE_FEEDING_CAMERA:
            exit_feeding_camera();
            break;

        case APP_STATE_MATH:
            exit_math();
            break;

        case APP_STATE_WESLANJE:
            exit_weslanje();
            break;

        case APP_STATE_PARENTAL_LOCK:
            exit_parental_lock();
            break;

        default:
            break;
    }
}

static void enter_state(app_state_t state)
{
    switch (state)
    {
        case APP_STATE_HOME_SCREEN:
            enter_home_screen();
            break;

        case APP_STATE_TURN_OFF:
            enter_turn_off();
            break;

        case APP_STATE_CAMERA:
            enter_camera();
            break;

        case APP_STATE_GALLERY:
            enter_gallery();
            break;

        case APP_STATE_CONTACTS:
            enter_contacts();
            break;

        case APP_STATE_CALL_SCREEN_1:
            enter_call_screen_1();
            break;

        case APP_STATE_CALL_SCREEN_2:
            enter_call_screen_2();
            break;

        case APP_STATE_CALL_SCREEN_3:
            enter_call_screen_3();
            break;

        case APP_STATE_CALL_SCREEN_4:
            enter_call_screen_4();
            break;

        case APP_STATE_INTERCOM:
            enter_intercom();
            break;

        case APP_STATE_MUSIC:
            enter_music();
            break;

        case APP_STATE_SONG_1:
            enter_song_1();
            break;

        case APP_STATE_SONG_2:
            enter_song_2();
            break;

        case APP_STATE_SONG_3:
            enter_song_3();
            break;

        case APP_STATE_FEEDING_CAMERA:
            enter_feeding_camera();
            break;

        case APP_STATE_MATH:
            enter_math();
            break;

        case APP_STATE_WESLANJE:
            enter_weslanje();
            break;

        case APP_STATE_PARENTAL_LOCK:
            enter_parental_lock();
            break;

        default:
            break;
    }
}

/* =========================
 * State entry implementations
 * ========================= */

static void enter_home_screen(void)
{
    lv_scr_load(ui_SimeAppsScr);
}

static void enter_turn_off(void)
{
    /* TODO:
     * - stop camera/music/intercom/call activity
     * - save anything needed
     * - turn display off / power down logic
     */
}

static void enter_camera(void)
{
    /* TODO:
     * - init/start camera preview
     * - show camera UI
     */
}

static void enter_gallery(void)
{
    /* TODO:
     * - stop camera preview if required
     * - load gallery data
     * - show gallery UI
     */
}

static void enter_contacts(void)
{
    lv_scr_load(ui_ContactsScr);
}

static void enter_call_screen_1(void)
{
    lv_scr_load(ui_CallScr);

    lv_obj_clear_flag(ui_MamaLabel2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_TataLabel2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_SpoodermanLabel2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_BarbiLabel2, LV_OBJ_FLAG_HIDDEN);

    audio_play(1, 20);

    for (int i = 0; i < 4; i++) {
        audio_play(2, 21);
        audio_play(2, 22);
        audio_play(2, 23);
    }
}

static void enter_call_screen_2(void)
{
    lv_scr_load(ui_CallScr);

    lv_obj_add_flag(ui_MamaLabel2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_TataLabel2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_SpoodermanLabel2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_BarbiLabel2, LV_OBJ_FLAG_HIDDEN);

    audio_play(1, 20);

    for (int i = 0; i < 4; i++) {
        audio_play(2, 21);
        audio_play(2, 21);
        audio_play(2, 23);
    }
}

static void enter_call_screen_3(void)
{
    lv_scr_load(ui_CallScr);

    lv_obj_add_flag(ui_MamaLabel2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_TataLabel2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_SpoodermanLabel2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_BarbiLabel2, LV_OBJ_FLAG_HIDDEN);

    audio_play(1, 20);

    for (int i = 0; i < 4; i++) {
        audio_play(2, 23);
        audio_play(2, 21);
        audio_play(2, 22);
    }
}

static void enter_call_screen_4(void)
{
    lv_scr_load(ui_CallScr);

    lv_obj_add_flag(ui_MamaLabel2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_TataLabel2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_SpoodermanLabel2, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_BarbiLabel2, LV_OBJ_FLAG_HIDDEN);

    audio_play(1, 20);

    for (int i = 0; i < 4; i++) {
        audio_play(2, 23);
        audio_play(2, 22);
        audio_play(2, 22);
    }
}

static void enter_intercom(void)
{
    /* TODO:
     * - start intercom audio path
     * - show intercom UI
     */
}

static void enter_music(void)
{
    (void)_curr_song;
}

static void enter_song_1(void)
{
    /* TODO:
     * - start playing song 1
     * - update UI
     */
}

static void enter_song_2(void)
{
    /* TODO:
     * - start playing song 2
     * - update UI
     */
}

static void enter_song_3(void)
{
    /* TODO:
     * - start playing song 3
     * - update UI
     */
}

static void enter_feeding_camera(void)
{
    /* TODO:
     * - start feeding camera stream
     * - show feeding camera UI
     */
}

static void enter_math(void)
{
    /* TODO:
     * - show math app UI
     */
}

static void enter_weslanje(void)
{
    lv_scr_load(ui_RowingGameScr);
    rowing_game_start();
}

static void enter_parental_lock(void)
{
    audio_stop();
    lv_scr_load(ui_ParentalLockScr);
}

/* =========================
 * State exit implementations
 * ========================= */

static void exit_home_screen(void)
{
    audio_stop();
}

static void exit_turn_off(void)
{
    /* TODO */
}

static void exit_camera(void)
{
    /* TODO:
     * - stop camera preview if needed
     */
}

static void exit_gallery(void)
{
    /* TODO */
}

static void exit_contacts(void)
{
    /* TODO */
}

static void exit_call_screen_1(void)
{
    audio_stop();
}

static void exit_call_screen_2(void)
{
    audio_stop();
}

static void exit_call_screen_3(void)
{
    audio_stop();
}

static void exit_call_screen_4(void)
{
    audio_stop();
}

static void exit_intercom(void)
{
    lv_scr_load(ui_IntercomScr);
}

static void exit_music(void)
{
}

static void exit_song_1(void)
{
    /* TODO:
     * - stop song 1 if needed
     */
}

static void exit_song_2(void)
{
    /* TODO:
     * - stop song 2 if needed
     */
}

static void exit_song_3(void)
{
    /* TODO:
     * - stop song 3 if needed
     */
}

static void exit_feeding_camera(void)
{
    /* TODO:
     * - stop feeding camera stream
     */
}

static void exit_math(void)
{
    /* TODO */
}

static void exit_weslanje(void)
{
    rowing_game_stop();
}

static void exit_parental_lock(void)
{
    
}