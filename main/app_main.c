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
#include "ui_app/wifi.h"
#include "ui_app/app_state.h"
#include "ui_app/sime.h"
#include "ui_app/rowing_game.h"
#include "ui_app/audio.h"
#include "ui_app/https_server.h"
#include "ui_app/intercom_audio.h"
#include "ui_app/parental_lock.h"

static const char *TAG = "MAIN";

static void sime_task(void *arg) {
    while (1) {
        sime_poll();
        // parental_lock_poll();

        // ESP_LOGI(TAG, "Sime status: HP=%d, Mood=%s",
        //          sime_get_hp(),
        //          sime_get_mood_str());

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init_sta();
    gui_init();
    rowing_game_init();
    audio_init();
    wifi_sync_time_from_network();
    
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

    ESP_ERROR_CHECK(https_server_start());
    ESP_ERROR_CHECK(intercom_audio_init());

   set_app_state(APP_STATE_HOME_SCREEN);
   parental_lock_init();

    xTaskCreate(
        sime_task,
        "sime_task",
        4096,
        NULL,
        5,
        NULL
    );
}