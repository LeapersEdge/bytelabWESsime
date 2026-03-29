
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "driver/i2s_std.h"
#include "intercom_audio.h"

#define INTERCOM_SAMPLE_RATE      16000
#define INTERCOM_BITS_PER_SAMPLE  16
#define INTERCOM_CHANNELS         1

#define INTERCOM_BCLK_IO          GPIO_NUM_26
#define INTERCOM_WS_IO            GPIO_NUM_27
#define INTERCOM_DOUT_IO          GPIO_NUM_14

#define INTERCOM_RINGBUF_SIZE     (8 * 1024)
#define INTERCOM_TASK_STACK       4096
#define INTERCOM_TASK_PRIO        5

static const char *TAG = "INTERCOM_AUDIO";

static RingbufHandle_t s_ringbuf = NULL;
static TaskHandle_t s_task = NULL;
static i2s_chan_handle_t s_tx_chan = NULL;
static volatile bool s_streaming = false;
static uint32_t s_drop_count = 0;

static esp_err_t intercom_i2s_init_if_needed(void)
{
    if (s_tx_chan != NULL) {
        return ESP_OK;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    esp_err_t err = i2s_new_channel(&chan_cfg, &s_tx_chan, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(err));
        s_tx_chan = NULL;
        return err;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(INTERCOM_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT,
            I2S_SLOT_MODE_MONO
        ),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = INTERCOM_BCLK_IO,
            .ws = INTERCOM_WS_IO,
            .dout = INTERCOM_DOUT_IO,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    err = i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_init_std_mode failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2s_channel_enable(s_tx_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_channel_enable failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2S initialized");
    return ESP_OK;
}

static void intercom_task(void *arg)
{
    (void)arg;

    while (1) {
        size_t item_size = 0;
        uint8_t *item = (uint8_t *)xRingbufferReceive(
            s_ringbuf,
            &item_size,
            pdMS_TO_TICKS(10)
        );

        if (!item) {
            continue;
        }

        if (s_streaming && s_tx_chan) {
            size_t bytes_written = 0;
            esp_err_t err = i2s_channel_write(
                s_tx_chan,
                item,
                item_size,
                &bytes_written,
                pdMS_TO_TICKS(5)
            );

            if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
                ESP_LOGE(TAG, "i2s_channel_write failed: %s", esp_err_to_name(err));
            }
        }

        vRingbufferReturnItem(s_ringbuf, item);
    }
}

esp_err_t intercom_audio_init(void)
{
    if (s_ringbuf) {
        return ESP_OK;
    }

    s_ringbuf = xRingbufferCreate(INTERCOM_RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (!s_ringbuf) {
        ESP_LOGE(TAG, "Failed to create ring buffer");
        return ESP_FAIL;
    }

    BaseType_t ok = xTaskCreate(
        intercom_task,
        "intercom_task",
        INTERCOM_TASK_STACK,
        NULL,
        INTERCOM_TASK_PRIO,
        &s_task
    );
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create intercom task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Intercom audio initialized");
    return ESP_OK;
}

esp_err_t intercom_audio_start(void)
{
    esp_err_t err = intercom_i2s_init_if_needed();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "intercom_i2s_init_if_needed failed: %s", esp_err_to_name(err));
        return err;
    }

    s_streaming = true;
    ESP_LOGI(TAG, "intercom_audio_start()");
    return ESP_OK;
}

esp_err_t intercom_audio_stop(void)
{
    s_streaming = false;
    ESP_LOGI(TAG, "intercom_audio_stop()");

    if (s_ringbuf) {
        size_t item_size = 0;
        void *item = NULL;

        while ((item = xRingbufferReceive(s_ringbuf, &item_size, 0)) != NULL) {
            vRingbufferReturnItem(s_ringbuf, item);
        }
    }

    return ESP_OK;
}

esp_err_t intercom_audio_enqueue(const uint8_t *data, size_t len)
{
    if (!s_streaming) {
        return ESP_OK;
    }

    if (!s_ringbuf || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    BaseType_t ok = xRingbufferSend(s_ringbuf, data, len, 0);
    if (ok != pdTRUE) {
        s_drop_count++;
        if ((s_drop_count % 50) == 0) {
            ESP_LOGW(TAG, "Dropped audio chunks: %lu", (unsigned long)s_drop_count);
        }
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
