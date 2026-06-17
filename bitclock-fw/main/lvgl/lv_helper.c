#include "lv_helper.h"

#include "esp_timer.h"
#include "lvgl/lvgl.h"

#include "views/aqi_grid.h"
#include "views/clock.h"
#include "views/logo.h"
#include "views/wifi_setup.h"

view_mode_t active_view_mode = VIEW_MODE_NONE;

uint32_t lv_tick_cb() { return esp_timer_get_time() / 1000; }

void lv_helper_set_view_mode(view_mode_t view_mode) {
  if (view_mode != active_view_mode) {
    lv_obj_t *screen = lv_scr_act();
    if (view_mode != VIEW_MODE_NONE) {
      lv_obj_clean(screen);
    }

    switch (view_mode) {
    case VIEW_MODE_NONE:
      break;
    case VIEW_MODE_LOGO:
      lv_helper_logo_create();
      break;
    case VIEW_MODE_CLOCK:
      lv_helper_clock_create();
      break;
    case VIEW_MODE_AQI_GRID:
      lv_helper_aqi_grid_create();
      break;
    case VIEW_MODE_WIFI_SETUP:
      lv_helper_wifi_setup_create(false);
      break;
    case VIEW_MODE_WIFI_SETUP_FALLBACK:
      lv_helper_wifi_setup_create(true);
      break;
    case VIEW_MODE_MAX:
      ESP_ERROR_CHECK(ESP_FAIL);
      break;
    }

    active_view_mode = view_mode;
  }
}

void lv_helper_update() {
  switch (active_view_mode) {
  case VIEW_MODE_NONE:
    break;
  case VIEW_MODE_LOGO:
    lv_helper_logo_update();
    break;
  case VIEW_MODE_CLOCK:
    lv_helper_clock_update(&lv_helper_view_mode_clock_data);
    break;
  case VIEW_MODE_AQI_GRID:
    lv_helper_aqi_grid_update(&lv_helper_view_mode_aqi_data);
    break;
  case VIEW_MODE_WIFI_SETUP:
  case VIEW_MODE_WIFI_SETUP_FALLBACK:
    lv_helper_wifi_setup_update();
    break;
  case VIEW_MODE_MAX:
    ESP_ERROR_CHECK(ESP_FAIL);
    break;
  }
}
