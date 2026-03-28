/**
* @file main.c
*
* @brief
*
* COPYRIGHT NOTICE: (c) 2022 Byte Lab Grupa d.o.o.
* All rights reserved.
*/
#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_netif_sntp.h"
#include "esp_log.h"
#include "gui.h"
#include "wifi.h"
#include "sime.h"

static const char *TAG = "MAIN";

static void sime_task(void *arg) {
    while (1) {
        sime_poll();

        ESP_LOGI(TAG, "Sime status: HP=%d, Mood=%s",
                 sime_get_hp(),
                 sime_get_mood_str());

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static volatile char test[3 * 240 * 320];

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init_sta();
    gui_init();
    wifi_sync_time_from_network();
   test[0] = '1';
    /*
     * Wait until network time becomes valid.
     */
    while (wifi_get_network_time() == 0) {
        ESP_LOGI(TAG, "Waiting for network time...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Network time ready: %lld",
             (long long)wifi_get_network_time());

    sime_init();

    ESP_LOGI(TAG, "Initial Sime status: HP=%d, Mood=%s",
             sime_get_hp(),
             sime_get_mood_str());

    /*
     * Optional test calls
     */
   //  sime_feed_half();
   //  sime_clean();

    xTaskCreate(
        sime_task,
        "sime_task",
        4096,
        NULL,
        5,
        NULL
    );
}