#include <stdbool.h>
#include <time.h>
#include "esp_log.h"
#include "sime.h"
#include "wifi.h"

#define SIME_MAX_HP 100
#define SIME_MIN_HP 0
#define SIME_SECONDS_PER_HP_LOSS 5
#define SIME_SECONDS_UNTIL_DIRTY 60

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
#define SIME_MOOD_VERY_HAPPY_LOWER_BOUND    75
#define SIME_MOOD_NEUTRAL_LOWER_BOUND       30
#define SIME_MOOD_UNHAPPY_LOWER_BOUND       15

static int _hp = SIME_MAX_HP;
static time_t _last_update_time = 0;
static sime_mood_t _last_mood = SIME_MOOD_VERY_HAPPY;
static bool _initialized = false;

static sime_clean_status_t _clean_status = SIME_CLEAN_STATUS_CLEAN;
static time_t _last_clean_time = 0;

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

    if ((now - _last_clean_time) >= SIME_SECONDS_UNTIL_DIRTY) {
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

void sime_init(void) {
    time_t now = wifi_get_network_time();

    if (now == 0) {
        ESP_LOGW(TAG, "Init skipped, network time not ready");
        return;
    }

    _hp = SIME_MAX_HP;
    _last_update_time = now;
    _last_mood = _calc_mood_from_hp(_hp);
    _clean_status = SIME_CLEAN_STATUS_CLEAN;
    _last_clean_time = now;
    _initialized = true;

    ESP_LOGI(TAG, "Initialized: hp=%d, mood=%s, clean=%s, time=%lld",
             _hp,
             _mood_to_str(_last_mood),
             _clean_status_to_str(_clean_status),
             (long long)now);
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
    ESP_LOGI(TAG, "Clean status requested: %s",
             _clean_status_to_str(_clean_status));
    return _clean_status;
}

const char *sime_get_clean_status_str(void) {
    const char *status_str = _clean_status_to_str(_clean_status);
    ESP_LOGI(TAG, "Clean status string requested: %s", status_str);
    return status_str;
}

void sime_feed_full(void) {
    int old_hp = _hp;
    _hp = _clamp_hp(_hp + 100);

    ESP_LOGI(TAG, "Feed full: hp %d -> %d", old_hp, _hp);
    _update_mood_log_if_changed();
}

void sime_feed_half(void) {
    int old_hp = _hp;
    _hp = _clamp_hp(_hp + 20);

    ESP_LOGI(TAG, "Feed half: hp %d -> %d", old_hp, _hp);
    _update_mood_log_if_changed();
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
}

void sime_poll(void) {
    time_t now = wifi_get_network_time();

    if (now == 0) {
        ESP_LOGW(TAG, "Poll skipped, network time not available");
        return;
    }

    if (!_initialized) {
        _hp = SIME_MAX_HP;
        _last_update_time = now;
        _last_mood = _calc_mood_from_hp(_hp);
        _clean_status = SIME_CLEAN_STATUS_CLEAN;
        _last_clean_time = now;
        _initialized = true;

        ESP_LOGI(TAG, "Auto-init in poll: hp=%d, mood=%s, clean=%s, time=%lld",
                 _hp,
                 _mood_to_str(_last_mood),
                 _clean_status_to_str(_clean_status),
                 (long long)now);
        return;
    }

    if (now > _last_update_time) {
        time_t elapsed_seconds = now - _last_update_time;

        int hp_loss = (int)(elapsed_seconds / SIME_SECONDS_PER_HP_LOSS);

        if (hp_loss > 0) {
            int old_hp = _hp;
            _hp = _clamp_hp(_hp - hp_loss);

            /*
             * Advance only by the amount of time that was actually converted into HP loss,
             * so fractional leftover time is preserved.
             */
            time_t consumed_seconds = (time_t)(hp_loss * SIME_SECONDS_PER_HP_LOSS);
            _last_update_time += consumed_seconds;

            ESP_LOGI(TAG, "Poll: elapsed=%lld s, hp loss=%d, hp %d -> %d",
                     (long long)elapsed_seconds,
                     hp_loss,
                     old_hp,
                     _hp);

            _update_mood_log_if_changed();
        }
    }

    _update_clean_status(now);
}
