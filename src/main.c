/**
 * ESP32-C6 Einfaches Blink-LED
 * 
 * Ganz einfaches Blink-Beispiel für den Anfang
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

// LED Pin (Port 15)
#define LED_PIN 15

static const char *TAG = "BLINK_LED";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-C6 Blink-LED gestartet");
    
    // LED Pin konfigurieren
    gpio_config_t led_config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&led_config);
    
    ESP_LOGI(TAG, "LED auf Pin %d initialisiert", LED_PIN);
    
    // Einfache Blink-Schleife
    while (1) {
        // LED einschalten
        gpio_set_level(LED_PIN, 1);
        ESP_LOGI(TAG, "LED AN");
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 Sekunde warten
        
        // LED ausschalten
        gpio_set_level(LED_PIN, 0);
        ESP_LOGI(TAG, "LED AUS");
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 Sekunde warten
    }
}
