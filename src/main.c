/**
 * ESP32-C6 WeatherstationLight
 * 
 * BME280 Sensor Test mit Blink-LED und WLAN-Verbindung
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "bme280.h"
#include "wifi_config.h"

// LED Pin (Port 15)
#define LED_PIN 15

static const char *TAG = "WEATHERSTATION";

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(5000));
    ESP_LOGI(TAG, "ESP32-C6 WeatherstationLight gestartet");
    
    // WLAN initialisieren und verbinden
    esp_err_t wifi_ret = wifi_init();
    if (wifi_ret == ESP_OK) {
        wifi_ret = wifi_connect();
        if (wifi_ret == ESP_OK) {
            ESP_LOGI(TAG, "WLAN erfolgreich verbunden!");
        } else {
            ESP_LOGE(TAG, "WLAN Verbindung fehlgeschlagen: %s", esp_err_to_name(wifi_ret));
        }
    } else {
        ESP_LOGE(TAG, "WLAN Initialisierung fehlgeschlagen: %s", esp_err_to_name(wifi_ret));
    }
    
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
        
        // Alle 10 Blinks: BME280 Messung und WLAN Status (falls verfügbar)
        if (blink_count % 10 == 0) {
            // WLAN Status prüfen
            if (wifi_is_connected()) {
                ESP_LOGI(TAG, "WLAN Status: VERBUNDEN");
            } else {
                ESP_LOGW(TAG, "WLAN Status: NICHT VERBUNDEN");
            }
            
            // BME280 Messung
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
