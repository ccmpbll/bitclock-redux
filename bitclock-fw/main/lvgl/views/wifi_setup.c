#include "wifi_setup.h"

#include "lvgl/lvgl.h"
#include "lvgl/styles.h"

// Screen is 264 wide x 176 tall. Center y is 88, so keep label offsets within
// roughly +/- 80 to avoid clipping at the top and bottom edges.

void lv_helper_wifi_setup_create(bool is_fallback) {
  lv_obj_t *screen = lv_screen_active();

  lv_obj_t *title = lv_label_create(screen);
  lv_obj_add_style(title, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -65);
  lv_label_set_text(title, is_fallback ? "WI-FI FAILED" : "WI-FI SETUP");

  lv_obj_t *l1 = lv_label_create(screen);
  lv_obj_add_style(l1, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(l1, LV_ALIGN_CENTER, 0, -33);
  lv_label_set_text(l1, "1. JOIN WIFI:");

  lv_obj_t *l2 = lv_label_create(screen);
  lv_obj_add_style(l2, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(l2, LV_ALIGN_CENTER, 0, -11);
  lv_label_set_text(l2, "BITCLOCK-SETUP");

  lv_obj_t *l3 = lv_label_create(screen);
  lv_obj_add_style(l3, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(l3, LV_ALIGN_CENTER, 0, 21);
  lv_label_set_text(l3, "2. VISIT:");

  lv_obj_t *l4 = lv_label_create(screen);
  lv_obj_add_style(l4, &sublabel_style, LV_PART_MAIN);
  lv_obj_align(l4, LV_ALIGN_CENTER, 0, 43);
  lv_label_set_text(l4, "192.168.4.1");
}

void lv_helper_wifi_setup_update() {}
