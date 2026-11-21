#include <pebble.h>

// Layout for emery (200x228). Bottom-most text ends at y=214, 14px margin.
#define LOCAL_TIME_Y   2   // Terminus 36, h 48
#define LOCAL_DATE_Y  50   // Terminus 18, h 22
#define EPOCH_LABEL_Y 88   // Terminus 14, h 16
#define EPOCH_Y       104  // Terminus 36 bold, h 40
#define UTC_LABEL_Y   152  // Terminus 14, h 16
#define UTC_TIME_Y    168  // Terminus 18, h 28
#define UTC_DATE_Y    196  // Terminus 18, h 18

static Window *s_window;
static TextLayer *s_local_date, *s_epoch_label, *s_utc_label, *s_utc_date;

// The three "hot" lines use custom layers so we can paint a phosphor glow.
typedef struct {
  char *text;
  GFont font;
  GSize size;
} GlowLine;

static Layer *s_local_time, *s_epoch, *s_utc_time;
static GlowLine s_local_time_line, s_epoch_line, s_utc_time_line;

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

  layer_mark_dirty(s_local_time);
  text_layer_set_text(s_local_date, s_local_date_buf);
  layer_mark_dirty(s_epoch);
  layer_mark_dirty(s_utc_time);
  text_layer_set_text(s_utc_date, s_utc_date_buf);
}

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  update_time();
}

static void draw_text_at(GContext *ctx, const GlowLine *line,
                         int dx, int dy, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, line->text, line->font,
                     GRect(dx, dy, line->size.w, line->size.h),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

// CRT bloom: dark corner bloom only, strokes stay sharp.
static void glow_update_proc(Layer *layer, GContext *ctx) {
  const GlowLine *line = layer_get_data(layer);
  static const int diag[4][2] = { {-1, -1}, {1, -1}, {-1, 1}, {1, 1} };

  for (int i = 0; i < 4; i++) {
    draw_text_at(ctx, line, diag[i][0], diag[i][1], GColorDarkGreen);
  }
  draw_text_at(ctx, line, 0, 0, GColorGreen);
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

static Layer *glow_layer_new(const GlowLine *src, GRect frame, GFont font) {
  Layer *layer = layer_create_with_data(frame, sizeof(GlowLine));
  GlowLine *line = layer_get_data(layer);
  *line = *src;
  line->font = font;
  line->size = frame.size;
  layer_set_update_proc(layer, glow_update_proc);
  layer_add_child(window_get_root_layer(s_window), layer);
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

  s_local_time_line.text = s_local_time_buf;
  s_local_time = glow_layer_new(&s_local_time_line,
                                GRect(0, LOCAL_TIME_Y, w, 48), s_font_local);
  s_local_date = text_layer_new(GRect(0, LOCAL_DATE_Y, w, 22),
                                s_font_date, GColorIslamicGreen);
  s_epoch_label = text_layer_new(GRect(0, EPOCH_LABEL_Y, w, 16),
                                 s_font_label, GColorIslamicGreen);
  text_layer_set_text(s_epoch_label, "UNIX TIME");
  s_epoch_line.text = s_epoch_buf;
  s_epoch = glow_layer_new(&s_epoch_line,
                           GRect(0, EPOCH_Y, w, 40), s_font_epoch);
  s_utc_label = text_layer_new(GRect(0, UTC_LABEL_Y, w, 16),
                               s_font_label, GColorIslamicGreen);
  text_layer_set_text(s_utc_label, "UTC");
  s_utc_time_line.text = s_utc_time_buf;
  s_utc_time = glow_layer_new(&s_utc_time_line,
                              GRect(0, UTC_TIME_Y, w, 28), s_font_utc);
  s_utc_date = text_layer_new(GRect(0, UTC_DATE_Y, w, 18),
                              s_font_date, GColorIslamicGreen);

  update_time();
}

static void window_unload(Window *window) {
  layer_destroy(s_local_time);
  layer_destroy(s_epoch);
  layer_destroy(s_utc_time);
  text_layer_destroy(s_local_date);
  text_layer_destroy(s_epoch_label);
  text_layer_destroy(s_utc_label);
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
