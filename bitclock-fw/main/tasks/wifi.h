#include "freertos/FreeRTOS.h"

#define WIFI_STACK_SIZE 1024 * 4

#define WIFI_READY_TO_CONNECT_EVENT BIT1
#define WIFI_AP_MODE_ACTIVE_EVENT BIT2
#define WIFI_AP_FALLBACK_EVENT BIT3
extern EventGroupHandle_t wifi_event_group_handle;

extern SemaphoreHandle_t wifi_req_semaphore;

extern StaticTask_t wifiTaskBuffer;
extern StackType_t wifiTaskStack[WIFI_STACK_SIZE];

void wifi_task_run(void *pvParameters);

bool bitclock_wifi_is_started();
bool bitclock_wifi_has_ip();
bool bitclock_wifi_has_sta_credentials();
