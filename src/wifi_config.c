/**
 * WLAN Konfiguration Implementation für ESP32-C6 WeatherstationLight
 */

#include "wifi_config.h"
#include "credentials.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "WIFI_CONFIG";

// Event Group für WLAN Status
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Statische Variablen für Verbindungsstatus
static int s_retry_num = 0;
static bool wifi_connected = false;
static esp_netif_t *s_netif = NULL;

/**
 * WLAN Event Handler
 */
void wifi_event_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "WLAN Verbindung fehlgeschlagen, Versuch %d/%d", s_retry_num, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "WLAN Verbindung endgültig fehlgeschlagen nach %d Versuchen", WIFI_MAX_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "WLAN verbunden! IP-Adresse: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        wifi_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/**
 * WLAN Initialisierung
 */
esp_err_t wifi_init(void)
{
    ESP_LOGI(TAG, "WLAN wird initialisiert...");
    
    // NVS initialisieren (wichtig für WLAN)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Event Group erstellen
    s_wifi_event_group = xEventGroupCreate();
    
    // Netzwerk Interface initialisieren
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    s_netif = esp_netif_create_default_wifi_sta();
    assert(s_netif);
    
    // WLAN initialisieren
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Event Handler registrieren
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    
    // WLAN Modus setzen
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    // WLAN Konfiguration setzen (BEVOR esp_wifi_start())
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // Hostname setzen
    ESP_ERROR_CHECK(wifi_set_hostname(WIFI_HOSTNAME));
    
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WLAN Initialisierung abgeschlossen");
    ESP_LOGI(TAG, "Hostname gesetzt: %s", WIFI_HOSTNAME);
    return ESP_OK;
}

/**
 * WLAN Verbindung herstellen
 */
esp_err_t wifi_connect(void)
{
    ESP_LOGI(TAG, "Verbinde mit WLAN: %s", WIFI_SSID);
    
    // Verbindung starten (Konfiguration wurde bereits in wifi_init() gesetzt)
    esp_wifi_connect();
    
    // Auf Verbindung warten
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT * 1000));
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WLAN erfolgreich verbunden!");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "WLAN Verbindung fehlgeschlagen!");
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "WLAN Verbindung Timeout!");
        return ESP_ERR_TIMEOUT;
    }
}

/**
 * WLAN Status abfragen
 */
bool wifi_is_connected(void)
{
    return wifi_connected;
}

/**
 * Hostname setzen
 */
esp_err_t wifi_set_hostname(const char* hostname)
{
    ESP_LOGI(TAG, "Setze Hostname: %s", hostname);
    
    // Hostname für das Netzwerk Interface setzen
    esp_err_t ret = esp_netif_set_hostname(s_netif, hostname);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fehler beim Setzen des Hostnames: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Zusätzlich für DHCP setzen (falls DHCP verwendet wird)
    esp_netif_dns_info_t dns_info;
    esp_netif_get_dns_info(s_netif, ESP_NETIF_DNS_MAIN, &dns_info);
    
    ESP_LOGI(TAG, "Hostname erfolgreich gesetzt: %s", hostname);
    return ESP_OK;
}

/**
 * WLAN deinitialisieren
 */
void wifi_cleanup(void)
{
    ESP_LOGI(TAG, "WLAN wird deinitialisiert...");
    esp_wifi_stop();
    esp_wifi_deinit();
    if (s_netif != NULL) {
        esp_netif_destroy(s_netif);
        s_netif = NULL;
    }
    vEventGroupDelete(s_wifi_event_group);
    ESP_LOGI(TAG, "WLAN deinitialisiert");
}
