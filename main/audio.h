#pragma once
#include "esp_err.h"

typedef enum {
    AUDIO_PRIORITY_NOTIFICATION    = 1,  // Highest
    AUDIO_PRIORITY_PHONE_CALL      = 2,
    AUDIO_PRIORITY_MUSIC           = 3   // Lowest
} AudioPriority;

void audio_init(void);
void audio_play(AudioPriority priority, int clip_id);   // 0 = notification, 1-3 = songs
void audio_stop(void);

bool is_audio_playing(void);
bool is_audio_with_id_playing(int clip_id);

extern float audio_master_volume;   // 0.0f ~ 1.0f