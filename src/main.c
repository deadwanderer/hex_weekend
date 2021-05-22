//------------------------------------------------------------------------------
//  clear-sapp.c
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_memtrack.h"
#include "sokol_debugtext.h"
#include "sokol_fetch.h"
#include "sokol_glue.h"
#include "cdbgui/cdbgui.h"

#include "stb/stb_image.h"

#include "HandmadeMath/HandmadeMath.h"
#include "Camera.h"
// #include "hex.h"

static void fetch_callback(const sfetch_response_t*);
static uint8_t favicon_buffer[32 * 32 * 4];

sg_pass_action pass_action;
const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;

void init(void) {
  sg_setup(&(sg_desc){.context = sapp_sgcontext()});
  sfetch_setup(
      &(sfetch_desc_t){.max_requests = 1, .num_channels = 1, .num_lanes = 1});
  pass_action =
      (sg_pass_action){.colors[0] = {.action = SG_ACTION_CLEAR,
                                     .value = {0.1f, 0.15f, 0.2f, 1.0f}}};
  sdtx_setup(&(sdtx_desc_t){
      .fonts[0] = sdtx_font_oric(),
  });
  __cdbgui_setup(sapp_sample_count());
  sfetch_send(&(sfetch_request_t){.path = "favicon-32x32.png",
                                  .callback = fetch_callback,
                                  .buffer_ptr = favicon_buffer,
                                  .buffer_size = sizeof(favicon_buffer)});
}

static void fetch_callback(const sfetch_response_t* response) {
  if (response->fetched) {
    int png_width, png_height, num_channels;
    const int desired_channels = 4;

    stbi_uc* pixels = stbi_load_from_memory(
        response->buffer_ptr, (int)response->fetched_size, &png_width,
        &png_height, &num_channels, desired_channels);
    if (pixels) {
      printf("Data size: %d\n", (int)response->fetched_size);
      printf("Image size: %dx%d\n", png_width, png_height);
      sapp_set_icon(&(sapp_icon_desc){
          .images = {
              {.width = 32,
               .height = 32,
               .pixels = {.ptr = pixels,
                          .size = (size_t)(png_width * png_height * 4)}}}});
    }
  }
}

void frame(void) {
  sfetch_dowork();
  // float g = pass_action.colors[0].value.g + 0.01f;
  // float b = pass_action.colors[0].value.b - 0.01f;
  // pass_action.colors[0].value.b = (b < 0.0f) ? 1.0f : b;
  // pass_action.colors[0].value.g = (g > 1.0f) ? 0.0f : g;

  const int w = sapp_width();
  const int h = sapp_height();

  // const float aspect = (float)w / (float)h;

  sdtx_canvas(w * 0.5f, h * 0.5f);
  sdtx_origin(2, 2);
  sdtx_puts("Sokol Header Allocations:\n\n");
  sdtx_printf("  Num: %d\n", smemtrack_info().num_allocs);
  sdtx_printf("  Allocs: %d bytes\n", smemtrack_info().num_bytes);

  sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
  sdtx_draw();
  __cdbgui_draw();
  sg_end_pass();
  sg_commit();
}

void event(const sapp_event* e) {
  if (e->type == SAPP_EVENTTYPE_KEY_DOWN &&
      e->key_code == SAPP_KEYCODE_ESCAPE) {
    sapp_request_quit();
  } else {
    __cdbgui_event(e);
  }
}

void cleanup(void) {
  __cdbgui_shutdown();
  sdtx_shutdown();
  sfetch_shutdown();
  sg_shutdown();
}

sapp_desc sokol_main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;
  return (sapp_desc){
      .init_cb = init,
      .frame_cb = frame,
      .cleanup_cb = cleanup,
      .event_cb = event,
      .width = SCREEN_WIDTH,
      .height = SCREEN_HEIGHT,
      .gl_force_gles2 = false,
      .window_title = "Hex_Weekend",
      .icon.sokol_default = false,
  };
}
