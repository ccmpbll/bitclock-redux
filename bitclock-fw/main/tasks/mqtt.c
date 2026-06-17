#include "mqtt.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "libs/nvs.h"
#include "libs/sensor_utils.h"
#include "mqtt_client.h"
#include "tasks/scd4x.h"
#include "tasks/sgp4x.h"
#include "tasks/sht4x.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "mqtt";
static esp_mqtt_client_handle_t client = NULL;
static bool connected = false;
static TaskHandle_t publish_task_handle = NULL;

#define PUBLISH_INTERVAL_MS 30000

static void get_device_id(char *buf, size_t len) {
  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  snprintf(buf, len, "bitclock_%02x%02x%02x%02x%02x%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void publish_discovery(esp_mqtt_client_handle_t c, const char *prefix) {
  char device_id[32];
  get_device_id(device_id, sizeof(device_id));

  char device_json[160];
  snprintf(device_json, sizeof(device_json),
           "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"Bitclock\","
           "\"manufacturer\":\"bitclock-redux\"}",
           device_id);

  struct {
    const char *key;
    const char *name;
    const char *unit;
    const char *device_class;
  } sensors[] = {
      {"temp",     "Temperature", "\xc2\xb0" "C", "temperature"},
      {"humidity", "Humidity",    "%",             "humidity"},
      {"co2",      "CO2",         "ppm",           "carbon_dioxide"},
      {"voc",      "VOC Index",   NULL,            NULL},
      {"nox",      "NOx Index",   NULL,            NULL},
  };

  for (int i = 0; i < 5; i++) {
    char disc_topic[128], state_topic[128], payload[512];
    snprintf(disc_topic, sizeof(disc_topic),
             "homeassistant/sensor/%s_%s/config", device_id, sensors[i].key);
    snprintf(state_topic, sizeof(state_topic), "%s/%s", prefix, sensors[i].key);

    char extras[96] = "";
    if (sensors[i].unit)
      snprintf(extras + strlen(extras), sizeof(extras) - strlen(extras),
               ",\"unit_of_measurement\":\"%s\"", sensors[i].unit);
    if (sensors[i].device_class)
      snprintf(extras + strlen(extras), sizeof(extras) - strlen(extras),
               ",\"device_class\":\"%s\"", sensors[i].device_class);

    snprintf(payload, sizeof(payload),
             "{\"name\":\"%s\",\"state_topic\":\"%s\","
             "\"unique_id\":\"%s_%s\",\"state_class\":\"measurement\"%s,%s}",
             sensors[i].name, state_topic,
             device_id, sensors[i].key,
             extras, device_json);

    esp_mqtt_client_publish(c, disc_topic, payload, 0, 1, 1);
  }
}

static void publish_state(esp_mqtt_client_handle_t c, const char *prefix) {
  float temp_c = sht4x_current_temp_celsius();
  float humidity = sht4x_current_relative_humidity();
  if (!isfinite(temp_c)) temp_c = 0;
  if (!isfinite(humidity)) humidity = 0;

  char topic[128], payload[32];

  snprintf(topic, sizeof(topic), "%s/temp", prefix);
  snprintf(payload, sizeof(payload), "%.1f", temp_c);
  esp_mqtt_client_publish(c, topic, payload, 0, 1, 0);

  snprintf(topic, sizeof(topic), "%s/humidity", prefix);
  snprintf(payload, sizeof(payload), "%.1f", humidity);
  esp_mqtt_client_publish(c, topic, payload, 0, 1, 0);

  snprintf(topic, sizeof(topic), "%s/co2", prefix);
  snprintf(payload, sizeof(payload), "%u", scd4x_current_co2_ppm());
  esp_mqtt_client_publish(c, topic, payload, 0, 1, 0);

  snprintf(topic, sizeof(topic), "%s/voc", prefix);
  snprintf(payload, sizeof(payload), "%ld", (long)sgp4x_current_voc_index());
  esp_mqtt_client_publish(c, topic, payload, 0, 1, 0);

  snprintf(topic, sizeof(topic), "%s/nox", prefix);
  snprintf(payload, sizeof(payload), "%ld", (long)sgp41_current_nox_index());
  esp_mqtt_client_publish(c, topic, payload, 0, 1, 0);
}

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
  const char *prefix = bitclock_nvs_get_mqtt_prefix();
  if (!prefix || !prefix[0]) prefix = "bitclock";

  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "Connected");
    connected = true;
    publish_discovery(event->client, prefix);
    publish_state(event->client, prefix);
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "Disconnected");
    connected = false;
    break;
  default:
    break;
  }
}

static void mqtt_publish_task(void *arg) {
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL_MS));
    if (!connected || !client) continue;
    const char *prefix = bitclock_nvs_get_mqtt_prefix();
    if (!prefix || !prefix[0]) prefix = "bitclock";
    publish_state(client, prefix);
  }
}

bool mqtt_is_connected() { return connected; }

static void mqtt_stop() {
  if (publish_task_handle) {
    vTaskDelete(publish_task_handle);
    publish_task_handle = NULL;
  }
  if (client) {
    esp_mqtt_client_stop(client);
    esp_mqtt_client_destroy(client);
    client = NULL;
    connected = false;
  }
}

static void mqtt_start_internal() {
  const char *host = bitclock_nvs_get_mqtt_host();
  if (!host || host[0] == '\0') {
    ESP_LOGI(TAG, "No broker configured, MQTT disabled");
    return;
  }

  uint16_t port = bitclock_nvs_get_mqtt_port();
  if (port == 0) port = 1883;

  char uri[128];
  snprintf(uri, sizeof(uri), "mqtt://%s:%u", host, port);

  esp_mqtt_client_config_t config = {};
  config.broker.address.uri = uri;

  const char *user = bitclock_nvs_get_mqtt_user();
  const char *pass = bitclock_nvs_get_mqtt_pass();
  if (user && user[0]) config.credentials.username = user;
  if (pass && pass[0]) config.credentials.authentication.password = pass;

  client = esp_mqtt_client_init(&config);
  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID,
                                 mqtt_event_handler, NULL);
  esp_mqtt_client_start(client);

  xTaskCreate(mqtt_publish_task, "mqtt_pub", 4096, NULL, 5,
              &publish_task_handle);
  ESP_LOGI(TAG, "Started, broker: %s", uri);
}

void mqtt_task_start() {
  static bool started = false;
  if (started) return;
  started = true;
  mqtt_start_internal();
}

void mqtt_task_restart() {
  mqtt_stop();
  mqtt_start_internal();
}
