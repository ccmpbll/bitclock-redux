#include "wifi.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "tasks/web_admin.h"
#include "tasks/wifi_ap.h"

static const char *TAG = "wifi";

#define WIFI_MAX_BOOT_RETRIES 10

EventGroupHandle_t wifi_event_group_handle;
static StaticEventGroup_t wifi_event_group;

StaticTask_t wifiTaskBuffer;
StackType_t wifiTaskStack[WIFI_STACK_SIZE];

SemaphoreHandle_t wifi_req_semaphore;
StaticSemaphore_t wifi_req_semaphore_mutex_buffer;

static bool wifi_started = false;
static bool wifi_has_ip_val = false;
static bool wifi_ever_had_ip = false;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  // We avoid any esp_wifi_*() calls directly here so we do
  // those in the context of the Wi-Fi task which claims the wifi_req_semaphore
  // in order to avoid race conditions with other tasks.
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
      xEventGroupSetBits(wifi_event_group_handle, WIFI_READY_TO_CONNECT_EVENT);
      break;
    case WIFI_EVENT_STA_DISCONNECTED:
      ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
      xEventGroupSetBits(wifi_event_group_handle, WIFI_READY_TO_CONNECT_EVENT);
      break;
    default:
      break;
    }
  } else if (event_base == IP_EVENT) {
    switch (event_id) {
    case IP_EVENT_STA_GOT_IP:
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
      wifi_has_ip_val = true;
      wifi_ever_had_ip = true;
      ESP_LOGI(TAG, "Connected with IP Address:" IPSTR,
               IP2STR(&event->ip_info.ip));
      // Idempotent — only starts the admin server + mDNS on first IP.
      web_admin_start();
      break;
    case IP_EVENT_STA_LOST_IP:
      wifi_has_ip_val = false;
      break;
    default:
      break;
    }
  }
}

static void wifi_driver_init() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  wifi_event_group_handle = xEventGroupCreateStatic(&wifi_event_group);

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));

  esp_netif_create_default_wifi_sta();
  esp_netif_create_default_wifi_ap();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_req_semaphore =
      xSemaphoreCreateMutexStatic(&wifi_req_semaphore_mutex_buffer);
}

bool bitclock_wifi_is_started() { return wifi_started; }

bool bitclock_wifi_has_ip() { return wifi_has_ip_val; }

bool bitclock_wifi_has_sta_credentials() {
  wifi_config_t cfg = {0};
  if (esp_wifi_get_config(WIFI_IF_STA, &cfg) != ESP_OK) {
    return false;
  }
  return cfg.sta.ssid[0] != '\0';
}

static void enter_ap_mode(bool is_fallback) {
  ESP_LOGI(TAG, "Entering AP provisioning mode (fallback=%d)", is_fallback);
  EventBits_t bits = WIFI_AP_MODE_ACTIVE_EVENT;
  if (is_fallback) {
    bits |= WIFI_AP_FALLBACK_EVENT;
  }
  xEventGroupSetBits(wifi_event_group_handle, bits);
  wifi_ap_start(is_fallback);
  // Block here — wifi_ap_start only returns after reboot (triggered inside
  // post_handler), so this task effectively sleeps until the device restarts.
  while (1) {
    vTaskDelay(portMAX_DELAY);
  }
}

void wifi_task_run(void *pvParameters) {
  wifi_driver_init();

  esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  esp_netif_sntp_init(&config);

  // If no credentials are saved, go straight to AP mode for first-time setup.
  if (!bitclock_wifi_has_sta_credentials()) {
    ESP_LOGI(TAG, "No Wi-Fi credentials found, starting AP setup");
    enter_ap_mode(false);
    return; // unreachable — enter_ap_mode blocks until reboot
  }

  // Credentials exist: start STA and attempt to connect.
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());
  wifi_started = true;

  EventBits_t wifi_event_bits = 0;
  int boot_retry_count = 0;

  while (1) {
    if (!xSemaphoreTake(wifi_req_semaphore, portMAX_DELAY)) {
      continue;
    }

    if (wifi_event_bits & WIFI_READY_TO_CONNECT_EVENT) {
      if (!wifi_ever_had_ip) {
        // Still in the boot-time connection window — enforce retry limit.
        if (boot_retry_count >= WIFI_MAX_BOOT_RETRIES) {
          xSemaphoreGive(wifi_req_semaphore);
          ESP_LOGW(TAG, "Exhausted %d boot retries, entering AP mode",
                   WIFI_MAX_BOOT_RETRIES);
          enter_ap_mode(true);
          return; // unreachable
        }
        boot_retry_count++;
        ESP_LOGI(TAG, "STA connect attempt %d/%d", boot_retry_count,
                 WIFI_MAX_BOOT_RETRIES);
      }
      esp_wifi_connect();
    }

    xSemaphoreGive(wifi_req_semaphore);

    wifi_event_bits = xEventGroupWaitBits(wifi_event_group_handle,
                                          WIFI_READY_TO_CONNECT_EVENT,
                                          true,  // clear on exit
                                          false, // wait for all
                                          portMAX_DELAY);
  }
}
