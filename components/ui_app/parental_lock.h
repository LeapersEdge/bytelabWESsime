#ifndef PARENTAL_LOCK_H
#define PARENTAL_LOCK_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t parental_lock_init(void);
esp_err_t parental_lock_save_schedule(int from_minutes, int to_minutes);
bool parental_lock_load_schedule(int *from_minutes, int *to_minutes);

bool parental_lock_time_is_valid(void);
bool parental_lock_should_be_locked_now(void);
void parental_lock_poll(void);

bool parental_lock_get_state(void);
void parental_lock_set_manual(bool locked);

#endif
