/**
* @file main.c

* @brief 
* 
* COPYRIGHT NOTICE: (c) 2022 Byte Lab Grupa d.o.o.
* All rights reserved.
*/

//--------------------------------- INCLUDES ----------------------------------
#include <math.h>
#include <stdio.h>
#include "gui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <esp_adc/adc_oneshot.h>
#include "sdkconfig.h"
//---------------------------------- MACROS -----------------------------------

#define LED_R_GPIO 26
#define LED_G_GPIO 27
#define LED_B_GPIO 14

#define BTN_1_GPIO 36
#define BTN_2_GPIO 32
#define BTN_3_GPIO 33
#define BTN_4_GPIO 25

#define JOY_X_GPIO 34
#define JOY_Y_GPIO 35

//-------------------------------- DATA TYPES ---------------------------------

//---------------------- PRIVATE FUNCTION PROTOTYPES --------------------------

void Configure_GPIO();

//------------------------- STATIC DATA & CONSTANTS ---------------------------

//------------------------------- GLOBAL DATA ---------------------------------

//------------------------------ PUBLIC FUNCTIONS -----------------------------
void app_main(void)
{
    gui_init();
    Configure_GPIO();

    int adc_joyx;
    int adc_joyy;
    float joyx;
    float joyy;
    adc_oneshot_unit_handle_t adc_handle;

    // Initialize ADC Oneshot Mode Driver on the ADC Unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
    
    // Configure ADC channel
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12, // 12 bit resolution, 0-4095
        .atten = ADC_ATTEN_DB_12   // ~3.3V full-scale voltage
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_6, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_7, &config));

    while (1) 
    {
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &adc_joyx));
        ESP_LOGI("ADC JOY X", "%d", adc_joyx);
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_7, &adc_joyy));
        ESP_LOGI("ADC JOY Y", "%d", adc_joyy);

        joyx = (adc_joyx-2048.0f)/2048.0f;
        joyy = (adc_joyy-2048.0f)/2048.0f;
        
        ESP_LOGI("GPIO", "joyx: %d\njoyy: %d\nbtn1: %d\nbtn2: %d\nbtn3: %d\nbtn4: %d\n\n",
                gpio_get_level(JOY_X_GPIO),
                gpio_get_level(JOY_Y_GPIO),
                gpio_get_level(BTN_1_GPIO),
                gpio_get_level(BTN_2_GPIO),
                gpio_get_level(BTN_3_GPIO),
                gpio_get_level(BTN_4_GPIO)
            );

        gpio_set_level(LED_R_GPIO, gpio_get_level(BTN_1_GPIO) || gpio_get_level(BTN_2_GPIO));
        gpio_set_level(LED_G_GPIO, gpio_get_level(BTN_3_GPIO) || gpio_get_level(BTN_4_GPIO));
        gpio_set_level(LED_B_GPIO, sqrt(joyy*joyy + joyx*joyx) >= 0.9f); 

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

//---------------------------- PRIVATE FUNCTIONS ------------------------------

void Configure_GPIO()
{
   ESP_LOGI("Configure GPIO", "configuring GPIO"); 
   ESP_LOGI("Configure GPIO", "configuring LEDs");

   gpio_reset_pin(LED_R_GPIO);
   gpio_set_direction(LED_R_GPIO, GPIO_MODE_OUTPUT);

   gpio_reset_pin(LED_G_GPIO);
   gpio_set_direction(LED_G_GPIO, GPIO_MODE_OUTPUT);

   gpio_reset_pin(LED_B_GPIO);
   gpio_set_direction(LED_B_GPIO, GPIO_MODE_OUTPUT);
   
   ESP_LOGI("Configure GPIO", "configuring BTNs");

   gpio_reset_pin(BTN_1_GPIO);
   gpio_set_direction(BTN_1_GPIO, GPIO_MODE_INPUT);

   gpio_reset_pin(BTN_2_GPIO);
   gpio_set_direction(BTN_2_GPIO, GPIO_MODE_INPUT);

   gpio_reset_pin(BTN_3_GPIO);
   gpio_set_direction(BTN_3_GPIO, GPIO_MODE_INPUT);

   gpio_reset_pin(BTN_4_GPIO);
   gpio_set_direction(BTN_4_GPIO, GPIO_MODE_INPUT);

   ESP_LOGI("Configure GPIO", "configuring JOYs");

   gpio_reset_pin(JOY_X_GPIO);
   gpio_set_direction(JOY_X_GPIO, GPIO_MODE_INPUT);

   gpio_reset_pin(JOY_Y_GPIO);
   gpio_set_direction(JOY_Y_GPIO, GPIO_MODE_INPUT);
   
   ESP_LOGI("Configure GPIO", "finished configuring GPIO");
}

//---------------------------- INTERRUPT HANDLERS -----------------------------

