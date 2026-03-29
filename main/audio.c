#include "audio.h"
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/ledc.h"

// ---------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------------- Macros ------------------------------------------------------

#define BUZZER_PIN          GPIO_NUM_2   // CHANGE TO THE GPIO YOU FOUND IN peripheral_module_schematic.PDF for LS1
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_RESOLUTION     LEDC_TIMER_10_BIT   // 1024 steps of volume control

// ---------------------------------------------------------------------------------------------------------------------
// -------------------------------------------------- Global variables -------------------------------------------------

float audio_master_volume = 0.7f;   // ← start low (0.3-0.5) and increase after testing
static QueueHandle_t audio_queue = NULL;

// Audio request struct
typedef struct {
    AudioPriority priority;
    int clip_id;
} AudioRequest_t;

// Note definition struct
typedef struct {
    float freq_hz;
    uint32_t duration_ms;
} Note_t;

static volatile int  current_playing_clip = -1;   // -1 = nothing playing
static volatile bool audio_is_playing_flag = false;

// ---------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------- System sounds ---------------------------------------------------

//static const Note_t notification_clip[] = {{440.0f, 200}, {329.63f, 200}, {523.0f, 200}};
static const Note_t notification_clip[] = {{440.0f, 80}, {493.88f, 80}, {554.37f, 100}};
static const int notification_len = 3;

static const Note_t click_sound[] = {{392.0f, 90}, {523.25f, 90}};
static const int click_sound_len = 2;

static const Note_t boot[] = {          // Cheerful ascending
    {261.63f, 300}, {293.66f, 300}, {329.63f, 300}, {349.23f, 300}, {392.00f, 400}
};
static const int boot_len = 5;

// --------------------------------------------------------------------------------------------------------------------
// --------------------------------------------------- Extra sounds ---------------------------------------------------

// static const Note_t song2[] = {          // Calm descending
//     {392.00f, 300}, {349.23f, 300}, {329.63f, 300}, {293.66f, 300}, {261.63f, 400}
// };
// static const int song2_len = 5;

// static const Note_t song3[] = {          // Energetic arpeggio
//     {261.63f, 250}, {329.63f, 250}, {392.00f, 250}, {523.25f, 250},
//     {392.00f, 300}, {329.63f, 300}
// };
// static const int song3_len = 6;

// ---------------------------------------------------------------------------------------------------------------------
// -------------------------------------------------- Music app sounds -------------------------------------------------

static const Note_t polka[] = {
    {392.00f, 88},
    {369.99f, 88},
    {392.00f, 88},
    {415.30f, 88},
    {440.00f, 88},
    {415.30f, 88},
    {440.00f, 88},
    {466.16f, 88},
    {493.88f, 176},
    {523.25f, 88},
    {100.0f, 88},
    {523.25f, 264},
    {100.0f, 264},
    {392.00f, 88},
    {392.00f, 88},
    {349.23f, 88},
    {100.0f, 88},
    {329.63f, 88},
    {100.0f, 88},
    {392.00f, 88},
    {392.00f, 88},
    {392.00f, 88},
    {100.0f, 88},
    {329.63f, 88},
    {329.63f, 88},
    {329.63f, 88},
    {100.0f, 264},
    {392.00f, 88},
    {392.00f, 88},
    {349.23f, 88},
    {100.0f, 88},
    {329.63f, 88},
    {100.0f, 88},
    {349.23f, 88},
    {349.23f, 88},
    {349.23f, 88},
    {100.0f, 88},
    {293.66f, 88},
    {293.66f, 88},
    {293.66f, 88},
    {100.0f, 264},
    {349.23f, 88},
    {349.23f, 88},
    {329.63f, 88},
    {100.0f, 88},
    {293.66f, 88},
    {100.0f, 88},
    {349.23f, 88},
    {349.23f, 88},
    {349.23f, 88},
    {100.0f, 88},
    {293.66f, 88},
    {293.66f, 88},
    {293.66f, 88},
    {100.0f, 88},
    {293.66f, 88},
    {293.66f, 88},
    {293.66f, 88},
    {293.66f, 88},
    {349.23f, 176},
    {329.63f, 88},
    {100.0f, 88},
    {440.00f, 88},
    {440.00f, 88},
    {440.00f, 88},
    {440.00f, 88},
    {392.00f, 88},
    {392.00f, 88},
    {392.00f, 88},
    {100.0f, 264},
    {392.00f, 88},
    {392.00f, 88},
    {349.23f, 88},
    {100.0f, 88},
    {329.63f, 88},
    {100.0f, 88},
    {392.00f, 88},
    {392.00f, 88},
    {392.00f, 88},
    {100.0f, 88},
    {329.63f, 88},
    {329.63f, 88},
    {329.63f, 88},
    {100.0f, 88},
    {392.00f, 88},
    {392.00f, 88},
    {392.00f, 88},
    {392.00f, 88},
    {392.00f, 88},
    {100.0f, 88},
    {523.25f, 88},
    {100.0f, 88},
    {493.88f, 88},
    {493.88f, 88},
    {493.88f, 88},
    {493.88f, 88},
    {493.88f, 176},
    {100.0f, 176},
    {493.88f, 88},
    {493.88f, 88},
    {493.88f, 88},
    {493.88f, 88},
    {523.25f, 88},
    {100.0f, 88},
    {587.33f, 88},
    {100.0f, 88},
    {523.25f, 88},
    {523.25f, 88},
    {523.25f, 88},
    {100.0f, 88},
    {493.88f, 88},
    {493.88f, 88},
    {493.88f, 88},
    {100.0f, 88},
    {440.00f, 88},
    {100.0f, 88},
    {392.00f, 88},
    {100.0f, 88},
    {440.00f, 88},
    {100.0f, 88},
    {493.88f, 88},
    {100.0f, 88},
    {523.25f, 176},
    {100.0f, 176},
    {392.00f, 176},
};

static const int polka_len =
    sizeof(polka) / sizeof(polka[0]);

static const Note_t elevator[] = {
    {392.00f, 166},
    {392.00f, 166},
    {392.00f, 166},
    {392.00f, 166},
    {392.00f, 166},
    {392.00f, 166},
    {392.00f, 166},
    {392.00f, 166},
    {466.16f, 166},
    {466.16f, 166},
    {466.16f, 166},
    {466.16f, 166},
    {369.99f, 166},
    {369.99f, 166},
    {369.99f, 166},
    {369.99f, 166},
    {349.23f, 166},
    {349.23f, 166},
    {349.23f, 166},
    {349.23f, 166},
    {349.23f, 166},
    {349.23f, 166},
    {349.23f, 166},
    {349.23f, 166},
    {392.00f, 166},
    {392.00f, 166},
    {392.00f, 166},
    {392.00f, 166},
    {311.13f, 166},
    {311.13f, 166},
    {311.13f, 166},
    {311.13f, 166},
    {261.63f, 166},
    {261.63f, 166},
    {261.63f, 166},
    {261.63f, 166},
    {293.66f, 166},
    {293.66f, 166},
    {293.66f, 166},
    {293.66f, 166},
    {311.13f, 166},
    {311.13f, 166},
    {311.13f, 166},
    {311.13f, 166},
    {246.94f, 166},
    {246.94f, 166},
    {246.94f, 166},
    {246.94f, 166},
    {293.66f, 166},
    {293.66f, 166},
    {293.66f, 166},
    {293.66f, 166},
    {293.66f, 166},
    {293.66f, 166},
    {293.66f, 166},
    {293.66f, 166},
    {88.0f, 333},
    {311.13f, 166},
    {311.13f, 166},
    {311.13f, 166},
    {311.13f, 166},
    {311.13f, 166},
    {311.13f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {311.13f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {116.54f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {311.13f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {116.54f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {311.13f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {116.54f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {392.00f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {116.54f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {311.13f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {116.54f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {311.13f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {116.54f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {311.13f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {116.54f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {392.00f, 166},
    {130.81f, 166},
    {116.54f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {88.0f, 166},
    {523.25f, 166},
    {783.99f, 166},
    {523.25f, 166},
    {783.99f, 166},
    {523.25f, 166},
    {783.99f, 166},
    {523.25f, 166},
    {783.99f, 166},
    {523.25f, 166},
    {88.0f, 166},
    {311.13f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {116.54f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {311.13f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {116.54f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {392.00f, 166},
    {523.25f, 166},
    {466.16f, 166},
    {349.23f, 166},
    {392.00f, 166},
    {116.54f, 166},
    {523.25f, 166},
    {783.99f, 166},
    {523.25f, 166},
    {783.99f, 166},
    {523.25f, 166},
    {783.99f, 166},
    {523.25f, 166},
    {783.99f, 166},
    {523.25f, 166},
    {88.0f, 166},
    {311.13f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {116.54f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {311.13f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {116.54f, 166},
    {392.00f, 166},
    {466.16f, 166},
    {392.00f, 166},
    {523.25f, 166},
    {116.54f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {88.0f, 166},
    {523.25f, 166},
    {783.99f, 166},
    {523.25f, 166},
    {783.99f, 166},
    {523.25f, 166},
    {783.99f, 166},
    {523.25f, 166},
    {783.99f, 166},
    {523.25f, 166},
    {88.0f, 166},
    {311.13f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {116.54f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {311.13f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {116.54f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {392.00f, 166},
    {523.25f, 166},
    {466.16f, 166},
    {349.23f, 166},
    {392.00f, 166},
    {116.54f, 166},
    {523.25f, 166},
    {783.99f, 166},
    {311.13f, 166},
    {523.25f, 166},
    {783.99f, 166},
    {523.25f, 166},
    {349.23f, 166},
    {783.99f, 166},
    {523.25f, 166},
    {783.99f, 166},
    {523.25f, 166},
    {88.0f, 166},
    {311.13f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {116.54f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {311.13f, 166},
    {130.81f, 166},
    {88.0f, 166},
    {349.23f, 166},
    {98.00f, 166},
    {116.54f, 166},
    {523.25f, 166},
    {523.25f, 166},
    {523.25f, 166},
    {523.25f, 166},
    {523.25f, 166},
    {523.25f, 166},
    {523.25f, 166},
};

static const int elevator_len = 256;

static const Note_t elise[] = {
    {659.26f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {493.88f, 306},
    {587.33f, 306},
    {523.25f, 306},
    {440.00f, 612},
    {220.00f, 306},
    {261.63f, 306},
    {329.63f, 306},
    {440.00f, 306},
    {493.88f, 612},
    {207.65f, 306},
    {329.63f, 306},
    {415.30f, 306},
    {493.88f, 306},
    {523.25f, 612},
    {220.00f, 306},
    {329.63f, 306},
    {659.26f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {493.88f, 306},
    {587.33f, 306},
    {523.25f, 306},
    {440.00f, 612},
    {220.00f, 306},
    {261.63f, 306},
    {329.63f, 306},
    {440.00f, 306},
    {493.88f, 612},
    {207.65f, 306},
    {329.63f, 306},
    {523.25f, 306},
    {493.88f, 306},
    {440.00f, 612},
    {220.00f, 306},
    {220.00f, 306},
    {659.26f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {493.88f, 306},
    {587.33f, 306},
    {523.25f, 306},
    {440.00f, 612},
    {220.00f, 306},
    {261.63f, 306},
    {329.63f, 306},
    {440.00f, 306},
    {493.88f, 612},
    {207.65f, 306},
    {329.63f, 306},
    {415.30f, 306},
    {493.88f, 306},
    {523.25f, 612},
    {220.00f, 306},
    {329.63f, 306},
    {659.26f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {493.88f, 306},
    {587.33f, 306},
    {523.25f, 306},
    {440.00f, 612},
    {220.00f, 306},
    {261.63f, 306},
    {329.63f, 306},
    {440.00f, 306},
    {493.88f, 612},
    {207.65f, 306},
    {329.63f, 306},
    {523.25f, 306},
    {493.88f, 306},
    {440.00f, 612},
    {220.00f, 306},
    {493.88f, 306},
    {523.25f, 306},
    {587.33f, 306},
    {659.26f, 918},
    {392.00f, 306},
    {698.46f, 306},
    {659.26f, 306},
    {587.33f, 918},
    {349.23f, 306},
    {659.26f, 306},
    {587.33f, 306},
    {523.25f, 918},
    {329.63f, 306},
    {587.33f, 306},
    {523.25f, 306},
    {493.88f, 612},
    {329.63f, 306},
    {329.63f, 306},
    {659.26f, 306},
    {329.63f, 306},
    {659.26f, 306},
    {659.26f, 306},
    {1318.51f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {493.88f, 306},
    {587.33f, 306},
    {523.25f, 306},
    {440.00f, 612},
    {220.00f, 306},
    {261.63f, 306},
    {329.63f, 306},
    {440.00f, 306},
    {493.88f, 612},
    {207.65f, 306},
    {329.63f, 306},
    {415.30f, 306},
    {493.88f, 306},
    {523.25f, 612},
    {220.00f, 306},
    {329.63f, 306},
    {659.26f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {622.25f, 306},
    {659.26f, 306},
    {493.88f, 306},
    {587.33f, 306},
    {523.25f, 306},
    {440.00f, 612},
    {220.00f, 306},
    {261.63f, 306},
    {329.63f, 306},
    {440.00f, 306},
    {493.88f, 612},
    {207.65f, 306},
    {329.63f, 306},
    {523.25f, 306},
    {493.88f, 306},
    {440.00f, 612},
    {220.00f, 306},
};

static const int elise_len = 154;

// ---------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------- Phone calls ----------------------------------------------------

static const Note_t ringtone[] = {
    {164.81f, 90},   // E3
    {207.65f, 120},  // G#3
    {329.63f, 90},   // E4
    {246.94f, 90},   // B3
    {277.18f, 90},   // C#4
    {207.65f, 120},  // G#3
    {246.94f, 90},   // B3
    {164.81f, 90},   // E3
    {207.65f, 120},  // G#3
    {329.63f, 90},   // E4
    {246.94f, 90},   // B3
    {207.65f, 150}   // G#3 (final)
};

static const int ringtone_len =
    sizeof(ringtone) / sizeof(ringtone[0]);

static const Note_t speech_1[] = {
    {146.83f, 150},  // D3
    {155.56f, 300},  // D#3
    {155.56f, 80},   // D#3
    {146.83f, 80},   // D3
    {174.61f, 120}   // F3
};

static const int speech_1_len =
    sizeof(speech_1) / sizeof(speech_1[0]);

static const Note_t speech_2[] = {
    {146.83f, 100},  // D3
    {164.81f, 100},  // E3
    {185.00f, 300},  // F#3
    {185.00f, 80},   // F#3
    {146.83f, 120}   // D3
};

static const int speech_2_len =
    sizeof(speech_2) / sizeof(speech_2[0]);

static const Note_t speech_3[] = {
    {146.83f, 100},  // D3
    {220.00f, 100},  // A3
    {246.94f, 100},  // B3
    {220.00f, 120},  // A3
    {277.18f, 210},  // C#4
    {146.83f, 80},   // D3
    {146.83f, 200}   // D3
};

static const int speech_3_len =
    sizeof(speech_3) / sizeof(speech_3[0]);

// ---------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------- Sime care sounds --------------------------------------------------

static const Note_t good_food[] = {
    {261.63f, 150}, // C4
    {329.63f, 150}, // E4
    {392.00f, 150}, // G4
    {523.25f, 300}  // C5
};
static const int good_food_len = 4;

// bad_food: C Eb Gb
static const Note_t bad_food[] = {
    {369.99f, 200},  // Gb4 (F#4)
    {311.13f, 200}, // Eb4 (D#4)
    {261.63f, 400} // C4
};
static const int bad_food_len = 3;

// clean: C F A higherC higherF
static const Note_t clean[] = {
    {261.63f, 150}, // C4
    {349.23f, 150}, // F4
    {440.00f, 150}, // A4
    {523.25f, 150}, // C5
    {698.46f, 300}  // F5
};
static const int clean_len = 5;

static volatile AudioPriority current_priority = 0;
static volatile bool stop_playback = false;

// ---------------------------------------------------------------------------------------------------------------------
// ------------------------------------------------- Program functions -------------------------------------------------

static void play_melody(const Note_t *notes, int len)
{
    stop_playback = false;
    for (int n = 0; n < len && !stop_playback; n++) {
        uint32_t freq = (uint32_t)notes[n].freq_hz;
        uint32_t duration_ticks = pdMS_TO_TICKS(notes[n].duration_ms);

        // Set frequency and volume (duty cycle scaled by master_volume)
        ledc_set_freq(LEDC_MODE, LEDC_TIMER, freq);
        uint32_t duty = (uint32_t)(512 * audio_master_volume);   // 50 % max duty
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

        vTaskDelay(duration_ticks);

        // Turn off tone between notes
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        vTaskDelay(pdMS_TO_TICKS(20));   // short musical gap
    }
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);   // ensure off at end
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

static void audio_task(void *arg)
{
    AudioRequest_t req;
    while (1) {
        if (xQueueReceive(audio_queue, &req, portMAX_DELAY) == pdTRUE) {
            if (req.priority < current_priority || current_priority == 0) {
                stop_playback = true;
                vTaskDelay(pdMS_TO_TICKS(10));
                current_priority = req.priority;

                const Note_t *clip = NULL;
                int clip_len = 0;
                switch (req.clip_id) {
                    // System sound effects
                    case 0: clip = notification_clip; clip_len = notification_len; break;
                    case 1: clip = click_sound;       clip_len = click_sound_len; break;
                    case 2: clip = boot;              clip_len = boot_len; break;

                    case 3: clip = song2;             clip_len = song2_len; break;
                    case 4: clip = song3;             clip_len = song3_len; break;
                    
                    // Music
                    case 5: clip = polka;           clip_len = polka_len; break;
                    case 6: clip = elevator;        clip_len = elevator_len; break;
                    case 7: clip = elise;           clip_len = elise_len; break;

                    // Sime care sound effects
                    case 10: clip = good_food;      clip_len = good_food_len; break;
                    case 11: clip = bad_food;       clip_len = bad_food_len; break;
                    case 12: clip = clean;          clip_len = clean_len; break;

                    // Ringtones
                    case 20: clip = ringtone;       clip_len = ringtone_len; break;
                    case 21: clip = speech_1;       clip_len = speech_1_len; break;
                    case 22: clip = speech_2;       clip_len = speech_2_len; break;
                    case 23: clip = speech_3;       clip_len = speech_3_len; break;
                    
                    default: current_priority = 0; continue;
                }

                // Audio clip is starting
                current_playing_clip = req.clip_id;
                audio_is_playing_flag = true;

                // Playing audio clip
                play_melody(clip, clip_len);

                // Audio clip is done playing
                current_playing_clip = -1;
                audio_is_playing_flag = false;
                current_priority = 0;
            }
        }
    }
}

void audio_init(void)
{
    // LEDC timer configuration (10-bit resolution, ~1-20 kHz range)
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_MODE,
        .duty_resolution = LEDC_RESOLUTION,
        .timer_num       = LEDC_TIMER,
        .freq_hz         = 1000,               // will be changed per note
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    // LEDC channel configuration on the buzzer pin
    ledc_channel_config_t channel_cfg = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = BUZZER_PIN,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_cfg));

    audio_queue = xQueueCreate(5, sizeof(AudioRequest_t));
    xTaskCreate(audio_task, "audio_task", 4096, NULL, 5, NULL);

    printf("Buzzer audio system ready (LS1 on GPIO %d, volume = %.1f)\n", BUZZER_PIN, audio_master_volume);
}

void audio_play(AudioPriority priority, int clip_id)
{
    AudioRequest_t req = {.priority = priority, .clip_id = clip_id};
    xQueueSend(audio_queue, &req, pdMS_TO_TICKS(10));
}

void audio_stop(void)
{
    stop_playback = true;

    current_playing_clip = -1;
    audio_is_playing_flag = false;
}

bool is_audio_playing(void)
{
    return audio_is_playing_flag;
}

bool is_audio_with_id_playing(int clip_id)
{
    return audio_is_playing_flag && (current_playing_clip == clip_id);
}