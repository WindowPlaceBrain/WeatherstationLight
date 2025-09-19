/**
 * BME280 Sensor Library for ESP32-C6 WeatherstationLight
 * 
 * Einfache Bibliothek für BME280 Temperatur, Luftdruck und Luftfeuchtigkeit Sensor
 * I2C Konfiguration: SDA=14, SCL=20
 */

#ifndef BME280_H
#define BME280_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"
#include "esp_err.h"

// BME280 I2C Konfiguration
#define BME280_I2C_PORT        I2C_NUM_0
#define BME280_SDA_PIN         14      // GPIO14 anpassen bei Bedarf
#define BME280_SCL_PIN         20      // GPIO20 anpassen bei Bedarf
#define BME280_I2C_FREQ        100000  // 100kHz
#define BME280_I2C_TIMEOUT     1000    // 1ms

// BME280 Register Adressen
#define BME280_ADDR            0x76    // I2C Adresse (0x76 oder 0x77)
#define BME280_REG_ID          0xD0    // Chip ID Register
#define BME280_REG_RESET       0xE0    // Reset Register
#define BME280_REG_CTRL_HUM    0xF2    // Humidity Control Register
#define BME280_REG_CTRL_MEAS   0xF4    // Control Measurement Register
#define BME280_REG_CONFIG      0xF5    // Configuration Register
#define BME280_REG_TEMP_MSB    0xFA    // Temperature MSB
#define BME280_REG_TEMP_LSB    0xFB    // Temperature LSB
#define BME280_REG_TEMP_XLSB   0xFC    // Temperature XLSB
#define BME280_REG_PRESS_MSB   0xF7    // Pressure MSB
#define BME280_REG_PRESS_LSB   0xF8    // Pressure LSB
#define BME280_REG_PRESS_XLSB  0xF9    // Pressure XLSB
#define BME280_REG_HUM_MSB     0xFD    // Humidity MSB
#define BME280_REG_HUM_LSB     0xFE    // Humidity LSB

// Kalibrierungsdaten Register (0x88-0xA1 und 0xE1-0xE7)
#define BME280_REG_CALIB_1     0x88    // T1-T3, P1-P9
#define BME280_REG_CALIB_2     0xE1    // H1, H2, H3

// BME280 Konfiguration (Forced Mode, optimiertes Oversampling)
#define BME280_CONFIG_HUM      0x01    // Humidity oversampling x1
#define BME280_CONFIG_MEAS     0x35    // Temperature x1, Pressure x4, Forced mode
#define BME280_CONFIG_FILTER   0x00    // Filter off, Standby 0.5ms

// Kalibrierungsdaten Struktur
typedef struct {
    // Temperatur Kalibrierung
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    
    // Luftdruck Kalibrierung
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    
    // Luftfeuchtigkeit Kalibrierung
    uint8_t  dig_H1;
    int16_t  dig_H2, dig_H3;
    int16_t  dig_H4, dig_H5;  // 12-bit signed Werte
    int8_t   dig_H6;
} bme280_calib_data_t;

// Messwerte Struktur
typedef struct {
    float temperature;    // °C
    float pressure;       // Pa
    float humidity;       // %
} bme280_data_t;

/**
 * @brief Initialisiert den BME280 Sensor
 * @return ESP_OK bei Erfolg, Fehlercode bei Fehler
 */
esp_err_t bme280_init(void);

/**
 * @brief Konfiguriert den BME280 Sensor
 * Setzt Forced Mode, 1x Oversampling, Filter Off
 * @return ESP_OK bei Erfolg, Fehlercode bei Fehler
 */
esp_err_t bme280_config(void);

/**
 * @brief Liest Kalibrierungsdaten aus dem Sensor
 * @param calib_data Pointer zur Kalibrierungsdaten Struktur
 * @return ESP_OK bei Erfolg, Fehlercode bei Fehler
 */
esp_err_t bme280_read_calib_data(bme280_calib_data_t *calib_data);

/**
 * @brief Startet eine Messung (Forced Mode)
 * @return ESP_OK bei Erfolg, Fehlercode bei Fehler
 */
esp_err_t bme280_start_measurement(void);

/**
 * @brief Liest Messwerte vom Sensor
 * @param data Pointer zur Datenstruktur
 * @return ESP_OK bei Erfolg, Fehlercode bei Fehler
 */
esp_err_t bme280_read_data(bme280_data_t *data);

/**
 * @brief Komplette Messung: Startet Messung und liest Werte
 * @param data Pointer zur Datenstruktur
 * @return ESP_OK bei Erfolg, Fehlercode bei Fehler
 */
esp_err_t bme280_measure(bme280_data_t *data);

#endif // BME280_H
