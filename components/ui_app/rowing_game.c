#include "rowing_game.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "./ui.h"

/* =========================
 * CONFIG DEFINES
 * ========================= */
#define ROWING_GAME_BUTTON_1_GPIO          GPIO_NUM_25
#define ROWING_GAME_BUTTON_2_GPIO          GPIO_NUM_32

#define ROWING_GAME_DURATION_MS            7000
#define ROWING_GAME_DEBOUNCE_MS            5

#define ROWING_GAME_TASK_STACK_SIZE        4096
#define ROWING_GAME_TASK_PRIORITY          5
#define ROWING_GAME_QUEUE_LEN              16

#define ROWING_GAME_UI_UPDATE_MS           100
#define ROWING_GAME_OAR_TOGGLE_ROWS        5

#define ROWING_GAME_NVS_NAMESPACE          "rowing_game"
#define ROWING_GAME_NVS_KEY_HIGH_SCORE     "high_score"

/*
 * Change this if your buttons are active high.
 * For normal ESP32 buttons wired to GND with pullups: keep 0.
 */
#define ROWING_GAME_BUTTON_ACTIVE_LEVEL    1

/* =========================
 * INTERNAL TYPES
 * ========================= */
typedef enum
{
    ROWING_EVENT_NONE = 0,
    ROWING_EVENT_BUTTON_1,
    ROWING_EVENT_BUTTON_2
} rowing_event_type_t;

typedef struct
{
    rowing_event_type_t type;
    TickType_t tick;
} rowing_event_t;

/* =========================
 * INTERNAL STATE
 * ========================= */
static const char *TAG = "ROWING_GAME";

static TaskHandle_t g_rowing_task_handle = NULL;
static QueueHandle_t g_rowing_event_queue = NULL;

static volatile bool g_initialized = false;
static volatile bool g_running = false;

static volatile uint32_t g_score = 0;
static volatile uint32_t g_high_score = 0;

static volatile int64_t g_end_time_us = 0;

/*
 * Cached display time.
 * Updated only every ROWING_GAME_UI_UPDATE_MS so button presses
 * do not refresh the visible timer.
 */
static volatile uint32_t g_display_time_left_ms = 0;

/*
 * Last accepted button in the rowing sequence:
 * 0 = none yet
 * 1 = last was button 1
 * 2 = last was button 2
 */
static volatile uint8_t g_last_button = 0;

/*
 * Oar state:
 * false = right visible, left hidden
 * true  = left visible, right hidden
 */
static volatile bool g_oars_swapped = false;

/* Debounce timestamps, stored in RTOS ticks */
static volatile TickType_t g_last_button_1_tick = 0;
static volatile TickType_t g_last_button_2_tick = 0;

/* =========================
 * FORWARD DECLARATIONS
 * ========================= */
static esp_err_t rowing_game_load_high_score(void);
static esp_err_t rowing_game_save_high_score(void);
static void rowing_game_finish_internal(void);
static void rowing_game_process_event(const rowing_event_t *event);
static void rowing_game_task(void *arg);
static void IRAM_ATTR rowing_game_button_isr(void *arg);
static void rowing_game_update_display_time(void);
static void rowing_game_update_ui(void);
static void rowing_game_set_oars(bool show_left);
static void rowing_game_update_oars_for_score(void);

/* =========================
 * NVS
 * ========================= */
static esp_err_t rowing_game_load_high_score(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(ROWING_GAME_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        g_high_score = 0;
        ESP_LOGI(TAG, "No rowing NVS namespace, high score = 0");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open read failed: %s", esp_err_to_name(err));
        return err;
    }

    {
        uint32_t value = 0;

        err = nvs_get_u32(nvs_handle, ROWING_GAME_NVS_KEY_HIGH_SCORE, &value);
        nvs_close(nvs_handle);

        if (err == ESP_ERR_NVS_NOT_FOUND) {
            g_high_score = 0;
            ESP_LOGI(TAG, "No saved high score, high score = 0");
            return ESP_OK;
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "nvs_get_u32 failed: %s", esp_err_to_name(err));
            return err;
        }

        g_high_score = value;
    }

    ESP_LOGI(TAG, "Loaded high score: %" PRIu32, g_high_score);
    return ESP_OK;
}

static esp_err_t rowing_game_save_high_score(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(ROWING_GAME_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open write failed: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u32(nvs_handle, ROWING_GAME_NVS_KEY_HIGH_SCORE, g_high_score);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_u32 failed: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Saved high score: %" PRIu32, g_high_score);
    return ESP_OK;
}

/* =========================
 * UI HELPERS
 * ========================= */
static void rowing_game_set_oars(bool show_left)
{
    if (show_left) {
        lv_obj_add_flag(ui_OarRightImg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_OarLeftImg, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(ui_OarRightImg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_OarLeftImg, LV_OBJ_FLAG_HIDDEN);
    }

    g_oars_swapped = show_left;
}

static void rowing_game_update_oars_for_score(void)
{
    bool show_left;

    if (ROWING_GAME_OAR_TOGGLE_ROWS == 0) {
        return;
    }

    /*
     * Toggle every N rows:
     * rows 0..4   -> right visible
     * rows 5..9   -> left visible
     * rows 10..14 -> right visible
     * ...
     */
    show_left = (((g_score / (uint32_t)ROWING_GAME_OAR_TOGGLE_ROWS) % 2U) != 0U);

    if (show_left != g_oars_swapped) {
        rowing_game_set_oars(show_left);
    }
}

static void rowing_game_update_display_time(void)
{
    int64_t now_us;

    if (!g_running) {
        g_display_time_left_ms = 0;
        return;
    }

    now_us = esp_timer_get_time();
    if (now_us >= g_end_time_us) {
        g_display_time_left_ms = 0;
        return;
    }

    g_display_time_left_ms = (uint32_t)((g_end_time_us - now_us) / 1000);
}

static void rowing_game_update_ui(void)
{
    uint32_t display_time_ds;

    display_time_ds = g_display_time_left_ms / 100U;

    /* TODO: GUI - update countdown using g_display_time_left_ms */
    /* TODO: GUI - update current score display using g_score */
    /* TODO: GUI - update high score display using g_high_score */

    lv_label_set_text_fmt(ui_RowingStatusLabel,
                          "%lu.%lu s\n%lu veslanja\nHS: %lu",
                          (unsigned long)(display_time_ds / 10U),
                          (unsigned long)(display_time_ds % 10U),
                          (unsigned long)g_score,
                          (unsigned long)g_high_score);
}

/* =========================
 * ISR
 * ========================= */
static void IRAM_ATTR rowing_game_button_isr(void *arg)
{
    uint32_t gpio_num;
    TickType_t now_tick;
    TickType_t debounce_ticks;
    rowing_event_t event;
    BaseType_t higher_priority_task_woken = pdFALSE;

    gpio_num = (uint32_t)(uintptr_t)arg;
    now_tick = xTaskGetTickCountFromISR();
    debounce_ticks = pdMS_TO_TICKS(ROWING_GAME_DEBOUNCE_MS);

    if (gpio_num == (uint32_t)ROWING_GAME_BUTTON_1_GPIO) {
        if ((now_tick - g_last_button_1_tick) < debounce_ticks) {
            return;
        }
        g_last_button_1_tick = now_tick;
        event.type = ROWING_EVENT_BUTTON_1;
    } else if (gpio_num == (uint32_t)ROWING_GAME_BUTTON_2_GPIO) {
        if ((now_tick - g_last_button_2_tick) < debounce_ticks) {
            return;
        }
        g_last_button_2_tick = now_tick;
        event.type = ROWING_EVENT_BUTTON_2;
    } else {
        return;
    }

    event.tick = now_tick;

    if (g_rowing_event_queue != NULL) {
        (void)xQueueSendFromISR(g_rowing_event_queue, &event, &higher_priority_task_woken);
    }

    if (higher_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/* =========================
 * GAME LOGIC
 * ========================= */
static void rowing_game_process_event(const rowing_event_t *event)
{
    if (!g_running) {
        return;
    }

    if (event->type == ROWING_EVENT_BUTTON_1) {
        if (g_last_button == 0U) {
            g_last_button = 1U;

            /* TODO: GUI - optionally show first side/button engaged */
            return;
        }

        if (g_last_button == 2U) {
            g_last_button = 1U;
            g_score++;

            rowing_game_update_oars_for_score();

            /* TODO: GUI - update current score display using g_score */
            /* Timer is intentionally NOT refreshed here */
            rowing_game_update_ui();
        }
    } else if (event->type == ROWING_EVENT_BUTTON_2) {
        if (g_last_button == 0U) {
            g_last_button = 2U;

            /* TODO: GUI - optionally show first side/button engaged */
            return;
        }

        if (g_last_button == 1U) {
            g_last_button = 2U;
            g_score++;

            rowing_game_update_oars_for_score();

            /* TODO: GUI - update current score display using g_score */
            /* Timer is intentionally NOT refreshed here */
            rowing_game_update_ui();
        }
    }
}

static void rowing_game_finish_internal(void)
{
    if (!g_running) {
        return;
    }

    g_running = false;
    g_end_time_us = 0;
    g_display_time_left_ms = 0;

    ESP_LOGI(TAG, "Game finished, final score=%" PRIu32, g_score);

    if (g_score > g_high_score) {
        g_high_score = g_score;
        ESP_LOGI(TAG, "New high score=%" PRIu32, g_high_score);

        if (rowing_game_save_high_score() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save high score");
        }

        /* TODO: GUI - show that a new high score was reached */
    }

    /* TODO: GUI - stop game screen updates */
    /* TODO: GUI - show final score from g_score */
    /* TODO: GUI - show high score from g_high_score */

    lv_label_set_text_fmt(ui_RowingStatusLabel,
                          "%lu.%lu s\n%lu veslanja\nHS: %lu",
                          0UL,
                          0UL,
                          (unsigned long)g_score,
                          (unsigned long)g_high_score);
}

static void rowing_game_task(void *arg)
{
    rowing_event_t event;
    TickType_t last_ui_update_tick;

    (void)arg;

    last_ui_update_tick = xTaskGetTickCount();

    while (1) {
        if (g_running) {
            TickType_t now_tick;

            now_tick = xTaskGetTickCount();

            if ((now_tick - last_ui_update_tick) >= pdMS_TO_TICKS(ROWING_GAME_UI_UPDATE_MS)) {
                rowing_game_update_display_time();

                if (g_display_time_left_ms == 0U) {
                    rowing_game_finish_internal();
                    last_ui_update_tick = xTaskGetTickCount();
                    continue;
                }

                rowing_game_update_ui();
                last_ui_update_tick = now_tick;
            }

            if (xQueueReceive(g_rowing_event_queue, &event, pdMS_TO_TICKS(10)) == pdTRUE) {
                rowing_game_process_event(&event);
            }

            if (g_running && (esp_timer_get_time() >= g_end_time_us)) {
                rowing_game_finish_internal();
                last_ui_update_tick = xTaskGetTickCount();
            }
        } else {
            /*
             * Background thread stays alive, but does nothing useful until start().
             * We still drain events so queue does not fill forever.
             */
            if (xQueueReceive(g_rowing_event_queue, &event, portMAX_DELAY) == pdTRUE) {
                /* ignore when not running */
            }

            last_ui_update_tick = xTaskGetTickCount();
        }
    }
}

/* =========================
 * PUBLIC API
 * ========================= */
esp_err_t rowing_game_init(void)
{
    esp_err_t err;
    gpio_config_t io_conf;

    if (g_initialized) {
        return ESP_OK;
    }

    memset(&io_conf, 0, sizeof(io_conf));
    io_conf.pin_bit_mask = (1ULL << ROWING_GAME_BUTTON_1_GPIO) |
                           (1ULL << ROWING_GAME_BUTTON_2_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = (ROWING_GAME_BUTTON_ACTIVE_LEVEL == 0) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = (ROWING_GAME_BUTTON_ACTIVE_LEVEL == 0) ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(err));
        return err;
    }

    /*
     * EXPLICIT GPIO INTERRUPT REGISTRATION:
     * 1. install ISR service
     * 2. select edge per pin
     * 3. add handler per pin
     */
    err = gpio_install_isr_service(0);
    if ((err != ESP_OK) && (err != ESP_ERR_INVALID_STATE)) {
        ESP_LOGE(TAG, "gpio_install_isr_service failed: %s", esp_err_to_name(err));
        return err;
    }

    if (ROWING_GAME_BUTTON_ACTIVE_LEVEL == 0) {
        err = gpio_set_intr_type(ROWING_GAME_BUTTON_1_GPIO, GPIO_INTR_NEGEDGE);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "gpio_set_intr_type btn1 failed: %s", esp_err_to_name(err));
            return err;
        }

        err = gpio_set_intr_type(ROWING_GAME_BUTTON_2_GPIO, GPIO_INTR_NEGEDGE);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "gpio_set_intr_type btn2 failed: %s", esp_err_to_name(err));
            return err;
        }
    } else {
        err = gpio_set_intr_type(ROWING_GAME_BUTTON_1_GPIO, GPIO_INTR_POSEDGE);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "gpio_set_intr_type btn1 failed: %s", esp_err_to_name(err));
            return err;
        }

        err = gpio_set_intr_type(ROWING_GAME_BUTTON_2_GPIO, GPIO_INTR_POSEDGE);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "gpio_set_intr_type btn2 failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    err = gpio_isr_handler_add(ROWING_GAME_BUTTON_1_GPIO,
                               rowing_game_button_isr,
                               (void *)(uintptr_t)ROWING_GAME_BUTTON_1_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add btn1 failed: %s", esp_err_to_name(err));
        return err;
    }

    err = gpio_isr_handler_add(ROWING_GAME_BUTTON_2_GPIO,
                               rowing_game_button_isr,
                               (void *)(uintptr_t)ROWING_GAME_BUTTON_2_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add btn2 failed: %s", esp_err_to_name(err));
        (void)gpio_isr_handler_remove(ROWING_GAME_BUTTON_1_GPIO);
        return err;
    }

    g_rowing_event_queue = xQueueCreate(ROWING_GAME_QUEUE_LEN, sizeof(rowing_event_t));
    if (g_rowing_event_queue == NULL) {
        (void)gpio_isr_handler_remove(ROWING_GAME_BUTTON_1_GPIO);
        (void)gpio_isr_handler_remove(ROWING_GAME_BUTTON_2_GPIO);
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_ERR_NO_MEM;
    }

    err = rowing_game_load_high_score();
    if (err != ESP_OK) {
        vQueueDelete(g_rowing_event_queue);
        g_rowing_event_queue = NULL;
        (void)gpio_isr_handler_remove(ROWING_GAME_BUTTON_1_GPIO);
        (void)gpio_isr_handler_remove(ROWING_GAME_BUTTON_2_GPIO);
        return err;
    }

    if (xTaskCreate(rowing_game_task,
                    "rowing_game",
                    ROWING_GAME_TASK_STACK_SIZE,
                    NULL,
                    ROWING_GAME_TASK_PRIORITY,
                    &g_rowing_task_handle) != pdPASS) {
        vQueueDelete(g_rowing_event_queue);
        g_rowing_event_queue = NULL;
        (void)gpio_isr_handler_remove(ROWING_GAME_BUTTON_1_GPIO);
        (void)gpio_isr_handler_remove(ROWING_GAME_BUTTON_2_GPIO);
        ESP_LOGE(TAG, "Failed to create rowing task");
        return ESP_ERR_NO_MEM;
    }

    g_score = 0U;
    g_running = false;
    g_end_time_us = 0;
    g_display_time_left_ms = 0U;
    g_last_button = 0U;
    g_last_button_1_tick = 0;
    g_last_button_2_tick = 0;
    g_oars_swapped = false;

    g_initialized = true;

    ESP_LOGI(TAG, "Rowing game initialized");
    ESP_LOGI(TAG, "Button 1 GPIO: %d", ROWING_GAME_BUTTON_1_GPIO);
    ESP_LOGI(TAG, "Button 2 GPIO: %d", ROWING_GAME_BUTTON_2_GPIO);
    ESP_LOGI(TAG, "Duration: %d ms", ROWING_GAME_DURATION_MS);
    ESP_LOGI(TAG, "Debounce: %d ms", ROWING_GAME_DEBOUNCE_MS);
    ESP_LOGI(TAG, "UI update: %d ms", ROWING_GAME_UI_UPDATE_MS);
    ESP_LOGI(TAG, "Oar toggle rows: %d", ROWING_GAME_OAR_TOGGLE_ROWS);
    ESP_LOGI(TAG, "High score: %" PRIu32, g_high_score);

    return ESP_OK;
}

esp_err_t rowing_game_deinit(void)
{
    if (!g_initialized) {
        return ESP_OK;
    }

    g_running = false;

    (void)gpio_isr_handler_remove(ROWING_GAME_BUTTON_1_GPIO);
    (void)gpio_isr_handler_remove(ROWING_GAME_BUTTON_2_GPIO);

    if (g_rowing_task_handle != NULL) {
        vTaskDelete(g_rowing_task_handle);
        g_rowing_task_handle = NULL;
    }

    if (g_rowing_event_queue != NULL) {
        vQueueDelete(g_rowing_event_queue);
        g_rowing_event_queue = NULL;
    }

    g_initialized = false;

    ESP_LOGI(TAG, "Rowing game deinitialized");
    return ESP_OK;
}

esp_err_t rowing_game_start(void)
{
    rowing_event_t dummy;

    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    g_score = 0U;
    g_last_button = 0U;
    g_last_button_1_tick = 0;
    g_last_button_2_tick = 0;
    g_oars_swapped = false;

    while (xQueueReceive(g_rowing_event_queue, &dummy, 0) == pdTRUE) {
    }

    g_end_time_us = esp_timer_get_time() + ((int64_t)ROWING_GAME_DURATION_MS * 1000LL);
    g_display_time_left_ms = ROWING_GAME_DURATION_MS;
    g_running = true;

    ESP_LOGI(TAG, "Rowing game started");

    /* Initial oar state: right visible, left hidden */
    rowing_game_set_oars(false);

    /* TODO: GUI - reset score display to 0 */
    /* TODO: GUI - show high score from g_high_score */
    /* TODO: GUI - start countdown for ROWING_GAME_DURATION_MS */

    rowing_game_update_ui();

    return ESP_OK;
}

esp_err_t rowing_game_stop(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    rowing_game_finish_internal();
    return ESP_OK;
}

bool rowing_game_is_running(void)
{
    return g_running;
}

uint32_t rowing_game_get_score(void)
{
    return g_score;
}

uint32_t rowing_game_get_high_score(void)
{
    return g_high_score;
}

uint32_t rowing_game_get_time_left_ms(void)
{
    int64_t now_us;

    if (!g_running) {
        return 0;
    }

    now_us = esp_timer_get_time();
    if (now_us >= g_end_time_us) {
        return 0;
    }

    return (uint32_t)((g_end_time_us - now_us) / 1000);
}