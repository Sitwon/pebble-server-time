#include <pebble.h>

// Layout for emery (200x228). Bottom-most text ends at y=214, 14px margin.
#define LOCAL_TIME_Y   2   // Bitham 42 Bold, h 48
#define LOCAL_DATE_Y  50   // Gothic 18 Bold, h 22
#define EPOCH_LABEL_Y 88   // Gothic 14, h 16
#define EPOCH_Y       104  // Gothic 28 Bold, h 34
#define UTC_LABEL_Y   152  // Gothic 14, h 16
#define UTC_TIME_Y    168  // Gothic 24 Bold, h 28
#define UTC_DATE_Y    196  // Gothic 14, h 18

static Window *s_window;
static TextLayer *s_local_time, *s_local_date;
static TextLayer *s_epoch_label, *s_epoch;
static TextLayer *s_utc_label, *s_utc_time, *s_utc_date;

static char s_local_time_buf[12], s_local_date_buf[16];
static char s_epoch_buf[16];
static char s_utc_time_buf[12], s_utc_date_buf[16];

static GFont s_font_local, s_font_epoch, s_font_utc, s_font_date, s_font_label;

// Strip the leading zero "12h" strftime gives ("01:23" -> "1:23").
static void strip_hour_zero(char *buf) {
  if (buf[0] == '0') {
    memmove(buf, buf + 1, strlen(buf));
  }
}

static void update_time(void) {
  time_t now = time(NULL); // Unix epoch seconds — the whole point of this face
  // pebble.h only exposes localtime/gmtime (static buffers, fine on a single-threaded face)
  struct tm *local_tm = localtime(&now);
  struct tm *utc_tm = gmtime(&now);

  strftime(s_local_time_buf, sizeof(s_local_time_buf),
           clock_is_24h_style() ? "%H:%M:%S" : "%I:%M:%S", local_tm);
  // 24h "00:03" is correct; only 12h strftime needs the leading zero removed
  if (!clock_is_24h_style()) {
    strip_hour_zero(s_local_time_buf);
  }
  strftime(s_local_date_buf, sizeof(s_local_date_buf), "%Y-%m-%d", local_tm);

  snprintf(s_epoch_buf, sizeof(s_epoch_buf), "%ld", (long)now);

  strftime(s_utc_time_buf, sizeof(s_utc_time_buf), "%H:%M:%S", utc_tm);
  strftime(s_utc_date_buf, sizeof(s_utc_date_buf), "%Y-%m-%d", utc_tm);

  text_layer_set_text(s_local_time, s_local_time_buf);
  text_layer_set_text(s_local_date, s_local_date_buf);
  text_layer_set_text(s_epoch, s_epoch_buf);
  text_layer_set_text(s_utc_time, s_utc_time_buf);
  text_layer_set_text(s_utc_date, s_utc_date_buf);
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  update_time();
}

static TextLayer *text_layer_new(GRect frame, GFont font, GColor color) {
  TextLayer *layer = text_layer_create(frame);
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_text_color(layer, color);
  text_layer_set_font(layer, font);
  text_layer_set_text_alignment(layer, GTextAlignmentCenter);
  layer_add_child(window_get_root_layer(s_window), text_layer_get_layer(layer));
  return layer;
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int w = bounds.size.w;

  // Terminus: true monospace, so digits never shift as they tick
  s_font_local = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TERM_36));
  s_font_epoch = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TERM_36_BOLD));
  s_font_utc = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TERM_18));
  s_font_date = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TERM_18));
  s_font_label = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TERM_14));

  s_local_time = text_layer_new(GRect(0, LOCAL_TIME_Y, w, 48),
                                s_font_local, GColorGreen);
  s_local_date = text_layer_new(GRect(0, LOCAL_DATE_Y, w, 22),
                                s_font_date, GColorIslamicGreen);
  s_epoch_label = text_layer_new(GRect(0, EPOCH_LABEL_Y, w, 16),
                                 s_font_label, GColorIslamicGreen);
  text_layer_set_text(s_epoch_label, "UNIX TIME");
  s_epoch = text_layer_new(GRect(0, EPOCH_Y, w, 40),
                           s_font_epoch, GColorGreen);
  s_utc_label = text_layer_new(GRect(0, UTC_LABEL_Y, w, 16),
                               s_font_label, GColorIslamicGreen);
  text_layer_set_text(s_utc_label, "UTC");
  s_utc_time = text_layer_new(GRect(0, UTC_TIME_Y, w, 28),
                              s_font_utc, GColorGreen);
  s_utc_date = text_layer_new(GRect(0, UTC_DATE_Y, w, 18),
                              s_font_date, GColorIslamicGreen);

  update_time();
}

static void window_unload(Window *window) {
  text_layer_destroy(s_local_time);
  text_layer_destroy(s_local_date);
  text_layer_destroy(s_epoch_label);
  text_layer_destroy(s_epoch);
  text_layer_destroy(s_utc_label);
  text_layer_destroy(s_utc_time);
  text_layer_destroy(s_utc_date);
  fonts_unload_custom_font(s_font_local);
  fonts_unload_custom_font(s_font_epoch);
  fonts_unload_custom_font(s_font_utc);
  fonts_unload_custom_font(s_font_date);
  fonts_unload_custom_font(s_font_label);
}

static void init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  // Green phosphor backlight while the face is foregrounded (auto-restored on exit)
  light_set_color_rgb888(0x0000FF00);

  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
