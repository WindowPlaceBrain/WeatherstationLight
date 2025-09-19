/**
 * BME280 Sensor Library Implementation
 * ESP32-C6 WeatherstationLight Project
 */

#include "bme280.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BME280";

// Globale Kalibrierungsdaten
static bme280_calib_data_t g_calib_data;
static bool g_calib_loaded = false;

/**
 * @brief I2C Schreibfunktion
 */
static esp_err_t bme280_i2c_write(uint8_t reg_addr, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME280_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(BME280_I2C_PORT, cmd, BME280_I2C_TIMEOUT / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief I2C Lesefunktion
 */
static esp_err_t bme280_i2c_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME280_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME280_ADDR << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(BME280_I2C_PORT, cmd, BME280_I2C_TIMEOUT / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief Temperatur Kompensation
 */
static int32_t bme280_compensate_temperature(int32_t adc_T, int32_t *t_fine)
{
    int32_t var1, var2, T;
    
    var1 = ((((adc_T >> 3) - ((int32_t)g_calib_data.dig_T1 << 1))) * ((int32_t)g_calib_data.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)g_calib_data.dig_T1)) * ((adc_T >> 4) - ((int32_t)g_calib_data.dig_T1))) >> 12) * ((int32_t)g_calib_data.dig_T3)) >> 14;
    *t_fine = var1 + var2;
    T = (*t_fine * 5 + 128) >> 8;
    return T;
}

/**
 * @brief Luftdruck Kompensation
 */
static uint32_t bme280_compensate_pressure(int32_t adc_P, int32_t t_fine)
{
    int64_t var1, var2, p;
    
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)g_calib_data.dig_P6;
    var2 = var2 + ((var1 * (int64_t)g_calib_data.dig_P5) << 17);
    var2 = var2 + (((int64_t)g_calib_data.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)g_calib_data.dig_P3) >> 8) + ((var1 * (int64_t)g_calib_data.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)g_calib_data.dig_P1) >> 33;
    
    if (var1 == 0) {
        return 0; // avoid exception caused by division by zero
    }
    
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)g_calib_data.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)g_calib_data.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)g_calib_data.dig_P7) << 4);
    
    return (uint32_t)p;
}

/**
 * @brief Luftfeuchtigkeit Kompensation
 */
static uint32_t bme280_compensate_humidity(int32_t adc_H, int32_t t_fine)
{
    int32_t v_x1_u32r;
    
    v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)g_calib_data.dig_H4) << 20) - (((int32_t)g_calib_data.dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)g_calib_data.dig_H6)) >> 10) * (((v_x1_u32r * ((int32_t)g_calib_data.dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)g_calib_data.dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)g_calib_data.dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
    
    return (uint32_t)(v_x1_u32r >> 12);
}

esp_err_t bme280_init(void)
{
    ESP_LOGI(TAG, "BME280 initialisieren...");
    
    // I2C Konfiguration
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BME280_SDA_PIN,
        .scl_io_num = BME280_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = BME280_I2C_FREQ,
    };
    
    esp_err_t ret = i2c_param_config(BME280_I2C_PORT, &i2c_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C Konfiguration fehlgeschlagen: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2c_driver_install(BME280_I2C_PORT, i2c_config.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C Driver Installation fehlgeschlagen: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // BME280 Chip ID prüfen
    uint8_t chip_id;
    ret = bme280_i2c_read(BME280_REG_ID, &chip_id, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BME280 nicht gefunden: %s", esp_err_to_name(ret));
        return ret;
    }
    
    if (chip_id != 0x60) {
        ESP_LOGE(TAG, "Falsche Chip ID: 0x%02X (erwartet: 0x60)", chip_id);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    ESP_LOGI(TAG, "BME280 gefunden (Chip ID: 0x%02X)", chip_id);
    
    // Kalibrierungsdaten lesen
    ret = bme280_read_calib_data(&g_calib_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Kalibrierungsdaten lesen fehlgeschlagen: %s", esp_err_to_name(ret));
        return ret;
    }
    
    g_calib_loaded = true;
    ESP_LOGI(TAG, "BME280 initialisiert!");
    return ESP_OK;
}

esp_err_t bme280_config(void)
{
    ESP_LOGI(TAG, "BME280 konfigurieren...");
    
    // Humidity oversampling x1
    uint8_t ctrl_hum = BME280_CONFIG_HUM;
    esp_err_t ret = bme280_i2c_write(BME280_REG_CTRL_HUM, &ctrl_hum, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Humidity Control schreiben fehlgeschlagen: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Temperature x1, Pressure x4, Forced mode
    uint8_t ctrl_meas = BME280_CONFIG_MEAS;
    ret = bme280_i2c_write(BME280_REG_CTRL_MEAS, &ctrl_meas, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Control Measurement schreiben fehlgeschlagen: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Filter off, Standby 0.5ms
    uint8_t config = BME280_CONFIG_FILTER;
    ret = bme280_i2c_write(BME280_REG_CONFIG, &config, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Configuration schreiben fehlgeschlagen: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "BME280 konfiguriert (Forced Mode, 1x Oversampling, Filter Off)");
    return ESP_OK;
}

esp_err_t bme280_read_calib_data(bme280_calib_data_t *calib_data)
{
    if (!calib_data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t calib_data1[26];
    uint8_t calib_data2[7];
    
    // Erste Gruppe Kalibrierungsdaten lesen (0x88-0xA1)
    esp_err_t ret = bme280_i2c_read(BME280_REG_CALIB_1, calib_data1, 26);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Zweite Gruppe Kalibrierungsdaten lesen (0xE1-0xE7)
    ret = bme280_i2c_read(BME280_REG_CALIB_2, calib_data2, 7);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Temperatur Kalibrierung
    calib_data->dig_T1 = (calib_data1[1] << 8) | calib_data1[0];
    calib_data->dig_T2 = (calib_data1[3] << 8) | calib_data1[2];
    calib_data->dig_T3 = (calib_data1[5] << 8) | calib_data1[4];
    
    // Luftdruck Kalibrierung
    calib_data->dig_P1 = (calib_data1[7] << 8) | calib_data1[6];
    calib_data->dig_P2 = (calib_data1[9] << 8) | calib_data1[8];
    calib_data->dig_P3 = (calib_data1[11] << 8) | calib_data1[10];
    calib_data->dig_P4 = (calib_data1[13] << 8) | calib_data1[12];
    calib_data->dig_P5 = (calib_data1[15] << 8) | calib_data1[14];
    calib_data->dig_P6 = (calib_data1[17] << 8) | calib_data1[16];
    calib_data->dig_P7 = (calib_data1[19] << 8) | calib_data1[18];
    calib_data->dig_P8 = (calib_data1[21] << 8) | calib_data1[20];
    calib_data->dig_P9 = (calib_data1[23] << 8) | calib_data1[22];
    
    // Luftfeuchtigkeit Kalibrierung
    calib_data->dig_H1 = calib_data1[25];
    calib_data->dig_H2 = (calib_data2[1] << 8) | calib_data2[0];
    calib_data->dig_H3 = calib_data2[2];
    // dig_H4 und dig_H5 sind 12-bit signed Werte
    calib_data->dig_H4 = (int16_t)((calib_data2[3] << 4) | (calib_data2[4] & 0x0F));
    calib_data->dig_H5 = (int16_t)((calib_data2[5] << 4) | (calib_data2[4] >> 4));
    
    // Sign-Extension für 12-bit zu 16-bit
    if (calib_data->dig_H4 & 0x800) calib_data->dig_H4 |= 0xF000;
    if (calib_data->dig_H5 & 0x800) calib_data->dig_H5 |= 0xF000;
    calib_data->dig_H6 = calib_data2[6];
    
    ESP_LOGI(TAG, "Kalibrierungsdaten gelesen");
    return ESP_OK;
}

esp_err_t bme280_start_measurement(void)
{
    if (!g_calib_loaded) {
        ESP_LOGE(TAG, "BME280 nicht initialisiert!");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Forced mode starten
    uint8_t ctrl_meas = BME280_CONFIG_MEAS;
    return bme280_i2c_write(BME280_REG_CTRL_MEAS, &ctrl_meas, 1);
}

esp_err_t bme280_read_data(bme280_data_t *data)
{
    if (!data || !g_calib_loaded) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t raw_data[8];
    esp_err_t ret = bme280_i2c_read(BME280_REG_PRESS_MSB, raw_data, 8);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Rohdaten extrahieren
    int32_t adc_P = (raw_data[0] << 12) | (raw_data[1] << 4) | (raw_data[2] >> 4);
    int32_t adc_T = (raw_data[3] << 12) | (raw_data[4] << 4) | (raw_data[5] >> 4);
    int32_t adc_H = (raw_data[6] << 8) | raw_data[7];
    
    // Kompensation
    int32_t t_fine;
    int32_t T = bme280_compensate_temperature(adc_T, &t_fine);
    uint32_t P = bme280_compensate_pressure(adc_P, t_fine);
    uint32_t H = bme280_compensate_humidity(adc_H, t_fine);
    
    // Werte konvertieren
    data->temperature = T / 100.0f;
    data->pressure = P / 256.0f;  // Pa
    data->humidity = H / 1024.0f; // % (bereits korrekt kompensiert)
    
    return ESP_OK;
}

esp_err_t bme280_measure(bme280_data_t *data)
{
    esp_err_t ret = bme280_start_measurement();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Warten bis Messung abgeschlossen (ca. 8ms)
    vTaskDelay(pdMS_TO_TICKS(10));
    
    return bme280_read_data(data);
}
