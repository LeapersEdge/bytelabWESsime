#include "parental_lock.h"

#include <time.h>
#include <stdbool.h>
#include <stdint.h>

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_err.h"

#include "app_state.h"

static const char *TAG = "PARENTAL_LOCK";

#define PARENTAL_LOCK_NVS_NS          "parent_lock"
#define PARENTAL_LOCK_KEY_FROM        "from_min"
#define PARENTAL_LOCK_KEY_TO          "to_min"
#define PARENTAL_LOCK_KEY_VALID       "sched_ok"

static int  s_from_minutes = -1;
static int  s_to_minutes   = -1;
static bool s_schedule_valid = false;
static bool s_locked = false;

static bool is_minutes_valid(int m)
{
    return (m >= 0) && (m < 24 * 60);
}

esp_err_t parental_lock_init(void)
{
    int from_minutes = -1;
    int to_minutes = -1;

    if (parental_lock_load_schedule(&from_minutes, &to_minutes)) {
        s_from_minutes = from_minutes;
        s_to_minutes = to_minutes;
        s_schedule_valid = true;
        // ESP_LOGI(TAG, "Loaded schedule: %d -> %d", s_from_minutes, s_to_minutes);
    } else {
        s_from_minutes = -1;
        s_to_minutes = -1;
        s_schedule_valid = false;
        // ESP_LOGI(TAG, "No valid parental lock schedule in NVS");
    }

    s_locked = (get_app_state() == APP_STATE_PARENTAL_LOCK);
    return ESP_OK;
}

esp_err_t parental_lock_save_schedule(int from_minutes, int to_minutes)
{
    if (!is_minutes_valid(from_minutes) || !is_minutes_valid(to_minutes)) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(PARENTAL_LOCK_NVS_NS, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_i32(nvs, PARENTAL_LOCK_KEY_FROM, from_minutes);
    if (err == ESP_OK) {
        err = nvs_set_i32(nvs, PARENTAL_LOCK_KEY_TO, to_minutes);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(nvs, PARENTAL_LOCK_KEY_VALID, 1);
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);

    if (err != ESP_OK) {
        return err;
    }

    s_from_minutes = from_minutes;
    s_to_minutes = to_minutes;
    s_schedule_valid = true;

    return ESP_OK;
}

bool parental_lock_load_schedule(int *from_minutes, int *to_minutes)
{
    if ((from_minutes == NULL) || (to_minutes == NULL)) {
        return false;
    }

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(PARENTAL_LOCK_NVS_NS, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return false;
    }

    int32_t from = -1;
    int32_t to = -1;
    uint8_t valid = 0;

    err = nvs_get_i32(nvs, PARENTAL_LOCK_KEY_FROM, &from);
    if (err == ESP_OK) {
        err = nvs_get_i32(nvs, PARENTAL_LOCK_KEY_TO, &to);
    }
    if (err == ESP_OK) {
        err = nvs_get_u8(nvs, PARENTAL_LOCK_KEY_VALID, &valid);
    }

    nvs_close(nvs);

    if ((err != ESP_OK) || (valid != 1)) {
        return false;
    }

    if (!is_minutes_valid((int)from) || !is_minutes_valid((int)to)) {
        return false;
    }

    *from_minutes = (int)from;
    *to_minutes = (int)to;
    return true;
}

bool parental_lock_time_is_valid(void)
{
    time_t now = 0;
    struct tm timeinfo = {0};

    time(&now);
    localtime_r(&now, &timeinfo);

    /* crude but effective:
       tm_year is years since 1900, so 2024 => 124 */
    return (timeinfo.tm_year >= 124);
}

bool parental_lock_should_be_locked_now(void)
{
    if (!s_schedule_valid) {
        return false;
    }

    if (!parental_lock_time_is_valid()) {
        return false;
    }

    time_t now = 0;
    struct tm timeinfo = {0};

    time(&now);
    localtime_r(&now, &timeinfo);

    int now_minutes = (timeinfo.tm_hour * 60) + timeinfo.tm_min;

    if (s_from_minutes == s_to_minutes) {
        /* interpret equal values as disabled */
        return false;
    }

    if (s_from_minutes < s_to_minutes) {
        /* same-day interval, e.g. 08:00 -> 20:00 */
        return (now_minutes >= s_from_minutes) && (now_minutes < s_to_minutes);
    } else {
        /* overnight interval, e.g. 22:00 -> 06:00 */
        return (now_minutes >= s_from_minutes) || (now_minutes < s_to_minutes);
    }
}

void parental_lock_poll(void)
{
    bool should_lock = parental_lock_should_be_locked_now();

    if (should_lock && !s_locked) {
        s_locked = true;
        set_app_state(APP_STATE_PARENTAL_LOCK);
    } else if (!should_lock && s_locked) {
        s_locked = false;
        set_app_state(APP_STATE_HOME_SCREEN);
    }
}

bool parental_lock_get_state(void)
{
    return s_locked;
}

void parental_lock_set_manual(bool locked)
{
    s_locked = locked;

    if (locked) {
        set_app_state(APP_STATE_PARENTAL_LOCK);
    } else {
        set_app_state(APP_STATE_HOME_SCREEN);
    }
}
