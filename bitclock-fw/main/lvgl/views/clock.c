#include "clock.h"

#include "aqi_grid.h"
#include "libs/nvs.h"
#include "libs/sensor_utils.h"
#include "lvgl/styles.h"
#include "lvgl/utils.h"
#include <math.h>
#include <time.h>

lv_helper_view_mode_clock_data_t lv_helper_view_mode_clock_data;

static lv_obj_t *climate_label; // temp + humidity, top row
static lv_obj_t *air_label;     // CO2 / VOC / NOx, second row
static lv_obj_t *time_label;
static lv_obj_t *time_sublabel;

void lv_helper_clock_create() {
  climate_label = lv_label_create(lv_screen_active());
  lv_obj_add_style(climate_label, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(climate_label, LV_ALIGN_TOP_MID, 0, 8);

  air_label = lv_label_create(lv_screen_active());
  lv_obj_add_style(air_label, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(air_label, LV_ALIGN_TOP_MID, 0, 30);

  time_label = lv_label_create(lv_screen_active());
  lv_obj_add_style(time_label, &large_number_style, LV_PART_MAIN);
  lv_obj_align(time_label, LV_ALIGN_CENTER, 0, 0);

  time_sublabel = lv_label_create(lv_screen_active());
  lv_obj_add_style(time_sublabel, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(time_sublabel, LV_ALIGN_CENTER, 0, 60);
}

void lv_helper_clock_update(lv_helper_view_mode_clock_data_t *data) {
  static struct tm timeinfo;
  localtime_r(&data->curtime, &timeinfo);

  static char time_label_str[6];
  static char time_sublabel_str[30];

  static char hour_str[3];
  static char minute_str[3];
  static char weekday_str[10];
  static char month_str[4];
  static char day_str[3];

  if (data->hour24) {
    snprintf(hour_str, sizeof(hour_str), "%u", timeinfo.tm_hour);
  } else {
    snprintf(hour_str, sizeof(hour_str), "%u",
             timeinfo.tm_hour % 12 == 0 ? 12 : timeinfo.tm_hour % 12);
  }
  strftime(minute_str, sizeof(minute_str), "%M", &timeinfo);
  snprintf(time_label_str, sizeof(time_label_str), "%s:%s", hour_str,
           minute_str);

  strftime(weekday_str, sizeof(weekday_str), "%A", &timeinfo);
  strftime(month_str, sizeof(month_str), "%b", &timeinfo);
  snprintf(day_str, sizeof(day_str), "%u", timeinfo.tm_mday);
  snprintf(time_sublabel_str, sizeof(time_sublabel_str), "%s · %s %s",
           weekday_str, month_str, day_str);
  str_to_upper(time_sublabel_str);

  set_text_if_changed(time_label, time_label_str);
  set_text_if_changed(time_sublabel, time_sublabel_str);

  // Sensor readings across the top, sourced from the shared aqi data that the
  // display task refreshes every frame.
  lv_helper_view_mode_aqi_data_t *aqi = &lv_helper_view_mode_aqi_data;
  bool fahrenheit =
      bitclock_nvs_get_temp_unit() != BITCLOCK_NVS_TEMP_UNIT_VAL_CELSIUS;
  float temp = fahrenheit ? celsius_to_fahrenheit(aqi->temp_celsius)
                          : aqi->temp_celsius;

  static char climate_str[24];
  snprintf(climate_str, sizeof(climate_str), "%.0f°  %u%%", roundf(temp),
           aqi->humidity_pct);
  set_text_if_changed(climate_label, climate_str);

  static char air_str[40];
  snprintf(air_str, sizeof(air_str), "CO₂ %u  VOC %ld  NOₓ %ld", aqi->co2_ppm,
           (long)aqi->voc_index, (long)aqi->nox_index);
  set_text_if_changed(air_label, air_str);
}
