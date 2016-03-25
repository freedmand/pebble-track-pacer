#include <pebble.h>
#include <math.h>
#include "track_display.h"

static Window *s_window;
static Layer* root_layer;
static AppTimer *timer;

static GFont s_res_gothic_18;

#define ROUND(x) ((int)round((x)))

#define TRACK_HEIGHT 164
#define TRACK_V_OFFSET 2
#define TRACK_H_OFFSET 5

#define STRAIGHTAWAY 84.39
#define INNER_SEMI_RADIUS 36.8
// 9-lane track
#define OUTER_SEMI_RADIUS 46.56
#define LANES_WIDTH (OUTER_SEMI_RADIUS - INNER_SEMI_RADIUS)
#define MID_SEMI_RADIUS (INNER_SEMI_RADIUS + LANES_WIDTH / 2)

#define CURVE_LENGTH (200 - STRAIGHTAWAY)

// 60 second 400m
#define PACE 20
#define DISTANCE 400

#define scale (TRACK_HEIGHT / (double)(STRAIGHTAWAY + OUTER_SEMI_RADIUS * 2))

#define TIMER_UPDATE_INTERVAL 30

static double current_meters;
static uint64_t start_time;

#define inner_100 GPoint(ROUND(TRACK_H_OFFSET + (OUTER_SEMI_RADIUS + INNER_SEMI_RADIUS) * scale), ROUND(TRACK_V_OFFSET + (OUTER_SEMI_RADIUS + STRAIGHTAWAY) * scale))
#define inner_200 GPoint(ROUND(TRACK_H_OFFSET + (OUTER_SEMI_RADIUS + INNER_SEMI_RADIUS) * scale), ROUND(TRACK_V_OFFSET + OUTER_SEMI_RADIUS * scale))
#define inner_300 GPoint(ROUND(TRACK_H_OFFSET + LANES_WIDTH * scale), ROUND(TRACK_V_OFFSET + OUTER_SEMI_RADIUS * scale))
#define inner_400 GPoint(ROUND(TRACK_H_OFFSET + LANES_WIDTH * scale), ROUND(TRACK_V_OFFSET + (OUTER_SEMI_RADIUS + STRAIGHTAWAY) * scale))
#define mid_100 GPoint(ROUND(TRACK_H_OFFSET + (OUTER_SEMI_RADIUS + MID_SEMI_RADIUS) * scale), ROUND(TRACK_V_OFFSET + (OUTER_SEMI_RADIUS + STRAIGHTAWAY) * scale))
#define mid_200 GPoint(ROUND(TRACK_H_OFFSET + (OUTER_SEMI_RADIUS + MID_SEMI_RADIUS) * scale), ROUND(TRACK_V_OFFSET + OUTER_SEMI_RADIUS * scale))
#define mid_300 GPoint(ROUND(TRACK_H_OFFSET + LANES_WIDTH / 2 * scale), ROUND(TRACK_V_OFFSET + OUTER_SEMI_RADIUS * scale))
#define mid_400 GPoint(ROUND(TRACK_H_OFFSET + LANES_WIDTH / 2 * scale), ROUND(TRACK_V_OFFSET + (OUTER_SEMI_RADIUS + STRAIGHTAWAY) * scale))
#define outer_100 GPoint(ROUND(TRACK_H_OFFSET + OUTER_SEMI_RADIUS * 2 * scale), ROUND(TRACK_V_OFFSET + (OUTER_SEMI_RADIUS + STRAIGHTAWAY) * scale))
#define outer_200 GPoint(ROUND(TRACK_H_OFFSET + OUTER_SEMI_RADIUS * 2 * scale), ROUND(TRACK_V_OFFSET + OUTER_SEMI_RADIUS * scale))
#define outer_300 GPoint(TRACK_H_OFFSET, ROUND(TRACK_V_OFFSET + OUTER_SEMI_RADIUS * scale))
#define outer_400 GPoint(TRACK_H_OFFSET, ROUND(TRACK_V_OFFSET + (OUTER_SEMI_RADIUS + STRAIGHTAWAY) * scale))

#define inner_200_circle GRect(inner_400.x, ROUND(inner_400.y - INNER_SEMI_RADIUS * scale), ROUND(INNER_SEMI_RADIUS * 2 * scale), ROUND(INNER_SEMI_RADIUS * 2 * scale))
#define inner_400_circle GRect(inner_300.x, ROUND(inner_300.y - INNER_SEMI_RADIUS * scale), ROUND(INNER_SEMI_RADIUS * 2 * scale), ROUND(INNER_SEMI_RADIUS * 2 * scale))
#define mid_200_circle GRect(mid_400.x, ROUND(mid_400.y - MID_SEMI_RADIUS * scale), ROUND(MID_SEMI_RADIUS * 2 * scale), ROUND(MID_SEMI_RADIUS * 2 * scale))
#define mid_400_circle GRect(mid_300.x, ROUND(mid_300.y - MID_SEMI_RADIUS * scale), ROUND(MID_SEMI_RADIUS * 2 * scale), ROUND(MID_SEMI_RADIUS * 2 * scale))
#define outer_200_circle GRect(outer_400.x, ROUND(outer_400.y - OUTER_SEMI_RADIUS * scale), ROUND(OUTER_SEMI_RADIUS * 2 * scale), ROUND(OUTER_SEMI_RADIUS * 2 * scale))
#define outer_400_circle GRect(outer_300.x, ROUND(outer_300.y - OUTER_SEMI_RADIUS * scale), ROUND(OUTER_SEMI_RADIUS * 2 * scale), ROUND(OUTER_SEMI_RADIUS * 2 * scale))

uint64_t get_time() {
  time_t t = 0;
  uint16_t t_ms = 0;
  time_ms(&t, &t_ms);
  
  uint64_t total_time = (uint64_t)t * 1000 + t_ms;
  return total_time;
}

static void draw_position(GContext *ctx, double meters) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  
  while (meters >= 400.0) {
    meters -= 400.0;
  }
  
  if (meters < CURVE_LENGTH) {
    double angle = 270.0 - ((double)meters / CURVE_LENGTH) * 180.0;
    graphics_fill_radial(ctx, outer_200_circle, GOvalScaleModeFitCircle, LANES_WIDTH, DEG_TO_TRIGANGLE(ROUND(angle)), DEG_TO_TRIGANGLE(270));
  } else {
    graphics_fill_radial(ctx, outer_200_circle, GOvalScaleModeFitCircle, LANES_WIDTH, DEG_TO_TRIGANGLE(90), DEG_TO_TRIGANGLE(270));
    if (meters < 200) {
      double position = ((double)meters - CURVE_LENGTH) / (200 - CURVE_LENGTH);
      int height = ROUND((inner_100.y - inner_200.y) * position);
      graphics_fill_rect(ctx, GRect(inner_100.x, inner_100.y - height, LANES_WIDTH, height), 0, 0);
    } else {
      graphics_fill_rect(ctx, GRect(inner_100.x, inner_200.y, LANES_WIDTH, inner_100.y - inner_200.y), 0, 0);
      if (meters < 200 + CURVE_LENGTH) {
        double angle = 90 - ((double)(meters - 200) / CURVE_LENGTH) * 180.0;
        graphics_fill_radial(ctx, outer_400_circle, GOvalScaleModeFitCircle, LANES_WIDTH, DEG_TO_TRIGANGLE(ROUND(angle)), DEG_TO_TRIGANGLE(90));
      } else {
        graphics_fill_radial(ctx, outer_400_circle, GOvalScaleModeFitCircle, LANES_WIDTH, DEG_TO_TRIGANGLE(-90), DEG_TO_TRIGANGLE(90));
        double position = ((double)meters - CURVE_LENGTH - 200) / (200 - CURVE_LENGTH);
        int height = ROUND((outer_400.y - outer_300.y) * position);
        graphics_fill_rect(ctx, GRect(outer_300.x, outer_300.y, LANES_WIDTH, height), 0, 0);
      }
    }
  }
}

static void update_track(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, 0);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_text_color(ctx, GColorBlack);
  
  char placeholder[10];
  snprintf(placeholder, sizeof(placeholder), "%d.%03d", (int)(current_meters), (int)(current_meters * 1000) % 1000);
  graphics_draw_text(ctx, placeholder, s_res_gothic_18, GRect(outer_200.x + 10, outer_200.y, 50, 20), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  int t = (int)(get_time() - start_time);
  snprintf(placeholder, sizeof(placeholder), "%d.%03d", t / 1000, t % 1000);
  graphics_draw_text(ctx, placeholder, s_res_gothic_18, GRect(outer_200.x + 10, outer_200.y + 50, 50, 20), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  

  // Draw straightaways.
  graphics_draw_line(ctx, inner_100, inner_200);
  graphics_draw_line(ctx, inner_300, inner_400);
  graphics_draw_line(ctx, outer_100, outer_200);
  graphics_draw_line(ctx, outer_300, outer_400);
  
  // Draw curves.
  graphics_draw_arc(ctx, inner_200_circle,
    GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(90), DEG_TO_TRIGANGLE(270));
  graphics_draw_arc(ctx, inner_400_circle,
    GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(-90), DEG_TO_TRIGANGLE(90));
  graphics_draw_arc(ctx, outer_200_circle,
    GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(90), DEG_TO_TRIGANGLE(270));
  graphics_draw_arc(ctx, outer_400_circle,
    GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(-90), DEG_TO_TRIGANGLE(90));
  
  draw_position(ctx, current_meters);
}

static void timer_callback(void *data) {
  layer_mark_dirty(root_layer);
  double elapsed_time = (int)(get_time() - start_time);
  current_meters = (double)elapsed_time / 1000.0 / PACE * DISTANCE;
  timer = app_timer_register(TIMER_UPDATE_INTERVAL, timer_callback, NULL);
}

static void initialise_ui(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  s_res_gothic_18 = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  
  // Get root layer.
  root_layer = window_get_root_layer(s_window);
  layer_set_update_proc(root_layer, update_track);
  
  layer_mark_dirty(root_layer);
  timer = app_timer_register(TIMER_UPDATE_INTERVAL, timer_callback, NULL);
}

static void destroy_ui(void) {
  window_destroy(s_window);
}

static void handle_window_unload(Window* window) {
  destroy_ui();
}

void show_track_display(void) {
  initialise_ui();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .unload = handle_window_unload,
  });
  window_stack_push(s_window, true);
}

void hide_track_display(void) {
  window_stack_remove(s_window, true);
}

int main(int argc, char** argv) {
  current_meters = 0;
  start_time = get_time();
  show_track_display();
  app_event_loop();
  hide_track_display();
}