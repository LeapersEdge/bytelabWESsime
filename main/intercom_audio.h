#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t intercom_audio_init(void);
esp_err_t intercom_audio_start(void);
esp_err_t intercom_audio_stop(void);
esp_err_t intercom_audio_enqueue(const uint8_t *data, size_t len);
