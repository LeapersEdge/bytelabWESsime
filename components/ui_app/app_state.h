#ifndef _APP_STATE_H
#define _APP_STATE_H

#include <stdbool.h>

typedef enum
{
    APP_STATE_HOME_SCREEN = 0,
    APP_STATE_TURN_OFF,
    APP_STATE_CAMERA,
    APP_STATE_GALLERY,
    APP_STATE_CONTACTS,
    APP_STATE_PARENTAL_LOCK,
    APP_STATE_CALL_SCREEN_1,
    APP_STATE_CALL_SCREEN_2,
    APP_STATE_CALL_SCREEN_3,
    APP_STATE_CALL_SCREEN_4,
    APP_STATE_INTERCOM,
    APP_STATE_MUSIC,
    APP_STATE_SONG_1,
    APP_STATE_SONG_2,
    APP_STATE_SONG_3,
    APP_STATE_FEEDING_CAMERA,
    APP_STATE_WESLANJE,
    APP_STATE_MATH
} app_state_t;

/*
 * Sets the application state.
 * This function is blocking and performs all logic needed
 * to transition from the current state to the new one.
 *
 * Returns true if transition was accepted/completed.
 * Returns false if the requested state is invalid.
 */
bool set_app_state(app_state_t new_state);

/*
 * Returns the current application state.
 */
app_state_t get_app_state(void);

/*
 * Returns the previous application state.
 */
app_state_t get_previous_app_state(void);

#endif