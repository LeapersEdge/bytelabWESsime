#ifndef ROWING_GAME_H
#define ROWING_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t rowing_game_init(void);
esp_err_t rowing_game_deinit(void);
esp_err_t rowing_game_start(void);
esp_err_t rowing_game_stop(void);

bool rowing_game_is_running(void);
uint32_t rowing_game_get_score(void);
uint32_t rowing_game_get_high_score(void);
uint32_t rowing_game_get_time_left_ms(void);

#endif