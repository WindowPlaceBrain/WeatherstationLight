/**
 * WLAN Konfiguration für ESP32-C6 WeatherstationLight
 * 
 * Diese Datei enthält die WLAN-Konfigurationsparameter
 */

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include "esp_wifi.h"
#include "esp_event.h"

// WLAN Event Handler
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// WLAN Initialisierung
esp_err_t wifi_init(void);

// WLAN Verbindung herstellen
esp_err_t wifi_connect(void);

// WLAN Status abfragen
bool wifi_is_connected(void);

// Hostname setzen
esp_err_t wifi_set_hostname(const char* hostname);

// WLAN deinitialisieren
void wifi_cleanup(void);

#endif // WIFI_CONFIG_H
