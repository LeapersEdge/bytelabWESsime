#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "esp_log.h"
#include "nvs.h"

#include "sime.h"
#include "wifi.h"

#define SIME_MAX_HP 100
#define SIME_MIN_HP 0

/*
 * Lose 1 HP every this many seconds.
 */
#define SIME_SECONDS_PER_HP_LOSS 5

/*
 * Become dirty after this many seconds since last clean.
 */
#define SIME_SECONDS_UNTIL_DIRTY 20

#define SIME_NVS_NAMESPACE          "sime"
#define SIME_NVS_KEY_HP             "hp"
#define SIME_NVS_KEY_LAST_HP_LOSS   "last_hp"
#define SIME_NVS_KEY_LAST_CLEAN     "last_clean"

static const char *TAG = "SIME";

/*
 * Mood lower bounds.
 * Evaluation order is top to bottom.
 *
 * hp >= VERY_HAPPY_LOWER_BOUND  => Very happy
 * hp >= NEUTRAL_LOWER_BOUND     => Neutral
 * hp >= UNHAPPY_LOWER_BOUND     => Unhappy
 * else                          => Very unhappy
 */
#define SIME_MOOD_VERY_HAPPY_LOWER_BOUND 75
#define SIME_MOOD_NEUTRAL_LOWER_BOUND    30
#define SIME_MOOD_UNHAPPY_LOWER_BOUND    15

static int _hp = SIME_MAX_HP;
static time_t _last_hp_loss_time = 0;
static time_t _last_clean_time = 0;
static sime_mood_t _last_mood = SIME_MOOD_VERY_HAPPY;
static sime_clean_status_t _clean_status = SIME_CLEAN_STATUS_CLEAN;
static bool _initialized = false;

static int _clamp_hp(int hp) {
    if (hp > SIME_MAX_HP) {
        return SIME_MAX_HP;
    }

    if (hp < SIME_MIN_HP) {
        return SIME_MIN_HP;
    }

    return hp;
}

static sime_mood_t _calc_mood_from_hp(int hp) {
    if (hp >= SIME_MOOD_VERY_HAPPY_LOWER_BOUND) {
        return SIME_MOOD_VERY_HAPPY;
    }

    if (hp >= SIME_MOOD_NEUTRAL_LOWER_BOUND) {
        return SIME_MOOD_NEUTRAL;
    }

    if (hp >= SIME_MOOD_UNHAPPY_LOWER_BOUND) {
        return SIME_MOOD_UNHAPPY;
    }

    return SIME_MOOD_VERY_UNHAPPY;
}

static const char *_mood_to_str(sime_mood_t mood) {
    switch (mood) {
        case SIME_MOOD_VERY_HAPPY:
            return "Very happy";
        case SIME_MOOD_NEUTRAL:
            return "Neutral";
        case SIME_MOOD_UNHAPPY:
            return "Unhappy";
        case SIME_MOOD_VERY_UNHAPPY:
            return "Very unhappy";
        default:
            return "Unknown";
    }
}

static const char *_clean_status_to_str(sime_clean_status_t status) {
    switch (status) {
        case SIME_CLEAN_STATUS_CLEAN:
            return "Clean";
        case SIME_CLEAN_STATUS_DIRTY:
            return "Dirty";
        default:
            return "Unknown";
    }
}

static void _update_mood_log_if_changed(void) {
    sime_mood_t new_mood = _calc_mood_from_hp(_hp);

    if (new_mood != _last_mood) {
        ESP_LOGI(TAG, "Mood changed: %s -> %s",
                 _mood_to_str(_last_mood),
                 _mood_to_str(new_mood));
        _last_mood = new_mood;
    }
}

static void _update_clean_status(time_t now) {
    sime_clean_status_t old_status = _clean_status;

    if (_last_clean_time == 0) {
        _clean_status = SIME_CLEAN_STATUS_CLEAN;
    } else if ((now - _last_clean_time) >= SIME_SECONDS_UNTIL_DIRTY) {
        _clean_status = SIME_CLEAN_STATUS_DIRTY;
    } else {
        _clean_status = SIME_CLEAN_STATUS_CLEAN;
    }

    if (_clean_status != old_status) {
        ESP_LOGI(TAG, "Clean status changed: %s -> %s",
                 _clean_status_to_str(old_status),
                 _clean_status_to_str(_clean_status));
    }
}

static esp_err_t _save_state_to_nvs(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(SIME_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for save: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_i32(nvs_handle, SIME_NVS_KEY_HP, _hp);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save %s: %s", SIME_NVS_KEY_HP, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_i64(nvs_handle, SIME_NVS_KEY_LAST_HP_LOSS, (int64_t)_last_hp_loss_time);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save %s: %s", SIME_NVS_KEY_LAST_HP_LOSS, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_i64(nvs_handle, SIME_NVS_KEY_LAST_CLEAN, (int64_t)_last_clean_time);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save %s: %s", SIME_NVS_KEY_LAST_CLEAN, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "Saved state: hp=%d, last_hp_loss=%lld, last_clean=%lld",
             _hp,
             (long long)_last_hp_loss_time,
             (long long)_last_clean_time);

    return ESP_OK;
}

static bool _load_state_from_nvs(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    int32_t hp = 0;
    int64_t last_hp_loss = 0;
    int64_t last_clean = 0;

    err = nvs_open(SIME_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS for load: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_get_i32(nvs_handle, SIME_NVS_KEY_HP, &hp);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Missing %s in NVS: %s", SIME_NVS_KEY_HP, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_get_i64(nvs_handle, SIME_NVS_KEY_LAST_HP_LOSS, &last_hp_loss);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Missing %s in NVS: %s", SIME_NVS_KEY_LAST_HP_LOSS, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_get_i64(nvs_handle, SIME_NVS_KEY_LAST_CLEAN, &last_clean);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Missing %s in NVS: %s", SIME_NVS_KEY_LAST_CLEAN, esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    nvs_close(nvs_handle);

    _hp = _clamp_hp((int)hp);
    _last_hp_loss_time = (time_t)last_hp_loss;
    _last_clean_time = (time_t)last_clean;

    ESP_LOGI(TAG, "Loaded state: hp=%d, last_hp_loss=%lld, last_clean=%lld",
             _hp,
             (long long)_last_hp_loss_time,
             (long long)_last_clean_time);

    return true;
}

static void _apply_elapsed_time(time_t now) {
    if (_last_hp_loss_time == 0) {
        _last_hp_loss_time = now;
    }

    if (_last_clean_time == 0) {
        _last_clean_time = now;
    }

    if (_last_hp_loss_time > now) {
        _last_hp_loss_time = now;
    }

    if (_last_clean_time > now) {
        _last_clean_time = now;
    }

    if (now > _last_hp_loss_time) {
        time_t elapsed_seconds = now - _last_hp_loss_time;
        int hp_loss = (int)(elapsed_seconds / SIME_SECONDS_PER_HP_LOSS);

        if (hp_loss > 0) {
            int old_hp = _hp;
            _hp = _clamp_hp(_hp - hp_loss);

            _last_hp_loss_time += (time_t)(hp_loss * SIME_SECONDS_PER_HP_LOSS);

            ESP_LOGI(TAG, "Applied elapsed time: elapsed=%lld s, hp loss=%d, hp %d -> %d",
                     (long long)elapsed_seconds,
                     hp_loss,
                     old_hp,
                     _hp);
        }
    }

    _last_mood = _calc_mood_from_hp(_hp);
    _update_clean_status(now);
}

void sime_init(void) {
    time_t now = wifi_get_network_time();

    if (now == 0) {
        ESP_LOGW(TAG, "Init skipped, network time not ready");
        return;
    }

    if (_load_state_from_nvs()) {
        _apply_elapsed_time(now);
        _initialized = true;

        ESP_LOGI(TAG, "Initialized from NVS: hp=%d, mood=%s, clean=%s",
                 _hp,
                 _mood_to_str(_last_mood),
                 _clean_status_to_str(_clean_status));

        (void)_save_state_to_nvs();
        return;
    }

    _hp = SIME_MAX_HP;
    _last_hp_loss_time = now;
    _last_clean_time = now;
    _last_mood = _calc_mood_from_hp(_hp);
    _clean_status = SIME_CLEAN_STATUS_CLEAN;
    _initialized = true;

    ESP_LOGI(TAG, "Initialized new Sime: hp=%d, mood=%s, clean=%s, time=%lld",
             _hp,
             _mood_to_str(_last_mood),
             _clean_status_to_str(_clean_status),
             (long long)now);

    (void)_save_state_to_nvs();
}

int sime_get_hp(void) {
    ESP_LOGI(TAG, "HP requested: %d", _hp);
    return _hp;
}

sime_mood_t sime_get_mood(void) {
    sime_mood_t mood = _calc_mood_from_hp(_hp);
    ESP_LOGI(TAG, "Mood requested: %s", _mood_to_str(mood));
    return mood;
}

const char *sime_get_mood_str(void) {
    const char *mood_str = _mood_to_str(_calc_mood_from_hp(_hp));
    ESP_LOGI(TAG, "Mood string requested: %s", mood_str);
    return mood_str;
}

sime_clean_status_t sime_get_clean_status(void) {
    ESP_LOGI(TAG, "Clean status requested: %s", _clean_status_to_str(_clean_status));
    return _clean_status;
}

const char *sime_get_clean_status_str(void) {
    const char *status_str = _clean_status_to_str(_clean_status);
    ESP_LOGI(TAG, "Clean status string requested: %s", status_str);
    return status_str;
}

void sime_feed_full(void) {
    time_t now = wifi_get_network_time();

    if (now == 0) {
        ESP_LOGW(TAG, "Feed full skipped, network time not available");
        return;
    }

    int old_hp = _hp;
    _hp = SIME_MAX_HP;
    _last_hp_loss_time = now;

    ESP_LOGI(TAG, "Feed full: hp %d -> %d", old_hp, _hp);

    _update_mood_log_if_changed();
    (void)_save_state_to_nvs();
}

void sime_feed_half(void) {
    time_t now = wifi_get_network_time();

    if (now == 0) {
        ESP_LOGW(TAG, "Feed half skipped, network time not available");
        return;
    }

    int old_hp = _hp;
    _hp = _clamp_hp(_hp + 20);

    /*
     * Preserve the newly gained HP over reset by moving the
     * reference time backwards from 'now' according to current HP.
     *
     * hp = MAX_HP - (now - last_hp_loss_time) / seconds_per_hp
     */
    _last_hp_loss_time = now - (time_t)((SIME_MAX_HP - _hp) * SIME_SECONDS_PER_HP_LOSS);

    ESP_LOGI(TAG, "Feed half: hp %d -> %d", old_hp, _hp);

    _update_mood_log_if_changed();
    (void)_save_state_to_nvs();
}

void sime_clean(void) {
    time_t now = wifi_get_network_time();

    if (now == 0) {
        ESP_LOGW(TAG, "Clean skipped, network time not available");
        return;
    }

    _last_clean_time = now;
    _clean_status = SIME_CLEAN_STATUS_CLEAN;

    ESP_LOGI(TAG, "Cleaned");

    (void)_save_state_to_nvs();
}

void sime_poll(void) {
    time_t now = wifi_get_network_time();

    if (now == 0) {
        ESP_LOGW(TAG, "Poll skipped, network time not available");
        return;
    }

    if (!_initialized) {
        sime_init();
        return;
    }

    if (now > _last_hp_loss_time) {
        time_t elapsed_seconds = now - _last_hp_loss_time;
        int hp_loss = (int)(elapsed_seconds / SIME_SECONDS_PER_HP_LOSS);

        if (hp_loss > 0) {
            int old_hp = _hp;
            _hp = _clamp_hp(_hp - hp_loss);

            _last_hp_loss_time += (time_t)(hp_loss * SIME_SECONDS_PER_HP_LOSS);

            ESP_LOGI(TAG, "Poll: elapsed=%lld s, hp loss=%d, hp %d -> %d",
                     (long long)elapsed_seconds,
                     hp_loss,
                     old_hp,
                     _hp);

            _update_mood_log_if_changed();
            (void)_save_state_to_nvs();
        }
    }

    _update_clean_status(now);
}
