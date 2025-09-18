/**
 * ESP32-C6 WeatherstationLight
 * 
 * BME280 Sensor Test mit Blink-LED
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "bme280.h"

// LED Pin (Port 15)
#define LED_PIN 15

static const char *TAG = "WEATHERSTATION";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-C6 WeatherstationLight gestartet");
    
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
    
    // BME280 Sensor initialisieren
    esp_err_t ret = bme280_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BME280 Initialisierung fehlgeschlagen: %s", esp_err_to_name(ret));
        ESP_LOGI(TAG, "Nur LED-Blink Modus aktiv");
    } else {
        // BME280 konfigurieren
        ret = bme280_config();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "BME280 Konfiguration fehlgeschlagen: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "BME280 bereit für Messungen");
        }
    }
    
    // Hauptschleife
    int blink_count = 0;
    while (1) {
        // LED blinken
        gpio_set_level(LED_PIN, 1);
        ESP_LOGI(TAG, "LED AN (Blink #%d)", ++blink_count);
        vTaskDelay(pdMS_TO_TICKS(500));
        
        gpio_set_level(LED_PIN, 0);
        ESP_LOGI(TAG, "LED AUS");
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Alle 10 Blinks: BME280 Messung (falls verfügbar)
        if (blink_count % 10 == 0) {
            bme280_data_t data;
            ret = bme280_measure(&data);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "BME280 Messung:");
                ESP_LOGI(TAG, "  Temperatur: %.2f °C", data.temperature);
                ESP_LOGI(TAG, "  Luftdruck:  %.2f hPa", data.pressure / 100.0f);
                ESP_LOGI(TAG, "  Luftfeuchtigkeit: %.2f %%", data.humidity);
            } else {
                ESP_LOGW(TAG, "BME280 Messung fehlgeschlagen: %s", esp_err_to_name(ret));
            }
        }
    }
}
