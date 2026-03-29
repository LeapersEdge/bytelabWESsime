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
#include "audio.h"
#include "driver/gpio.h"

#define BUTTON_PIN   GPIO_NUM_25   // ← REPLACE with the exact GPIO from peripheral_module_schematic.PDF for BTN4
// Usually active-low with internal pull-up

static void button_task(void *arg)
{

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    
    //bool last_state = 1;        // start assuming not pressed (high)
    last_state = gpio_get_level(BUTTON_PIN); // immediately read button state on start
    while (1) {
        bool current = gpio_get_level(BUTTON_PIN);

        // Detect falling edge (press) with simple debounce
        if (current != last_state) {
            vTaskDelay(pdMS_TO_TICKS(50));           // debounce time
            current = gpio_get_level(BUTTON_PIN);    // re-read after debounce

            if (current != last_state) {             // still changed → valid edge
                last_state = current;

                if (current == 0) {                  // BTN4 pressed (active-low)
                    //audio_play(AUDIO_PRIORITY_MUSIC, 1);   // Song 1 at music priority
                    audio_play(AUDIO_PRIORITY_NOTIFICATION, 7);
                    // You can change to AUDIO_PRIORITY_NOTIFICATION, 0 for the notification beep instead
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));   // polling rate (fast enough, low CPU)
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());

    gui_init();

    audio_init();
    audio_play(AUDIO_PRIORITY_NOTIFICATION, 2);   // 2 = boot sound clip

    // Task to play audio
    xTaskCreate(button_task, "btn_task", 4096, NULL, 4, NULL);

}