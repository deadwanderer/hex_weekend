//------------------------------------------------------------------------------
//  clear-sapp.c
//------------------------------------------------------------------------------
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_time.h"
#include "sokol_memtrack.h"
#include "sokol_debugtext.h"
#include "sokol_fetch.h"
#include "sokol_glue.h"
#include "cdbgui/cdbgui.h"
#include "HandmadeMath/HandmadeMath.h"
#include "shaders.glsl.h"

#include "stb/stb_image.h"

#include "Camera.h"
// #include "hex.h"

static void fetch_callback(const sfetch_response_t*);
static uint8_t favicon_buffer[32 * 32 * 4];

enum INPUTS {
  INPUT_W,
  INPUT_S,
  INPUT_A,
  INPUT_D,
  INPUT_LEFT,
  INPUT_RIGHT,
  INPUT_NUM
};

static struct {
  sg_pipeline pip;
  sg_bindings bind;
  camera_t cam;
  hmm_vec2 last_mouse;
  bool first_mouse;
  uint64_t lastFrameTime;
} state;

sg_pass_action pass_action;
const int SCREEN_WIDTH = 1920;
const int SCREEN_HEIGHT = 1080;
const float VELOCITY = 25.0f;

void init(void) {
  sg_setup(&(sg_desc){.context = sapp_sgcontext()});
  sfetch_setup(
      &(sfetch_desc_t){.max_requests = 1, .num_channels = 1, .num_lanes = 1});
  stm_setup();
  state.lastFrameTime = stm_now();
  pass_action =
      (sg_pass_action){.colors[0] = {.action = SG_ACTION_CLEAR,
                                     .value = {0.1f, 0.15f, 0.2f, 1.0f}}};
  sdtx_setup(&(sdtx_desc_t){
      .fonts[0] = sdtx_font_oric(),
  });
  __cdbgui_setup(sapp_sample_count());

  float vertices[] = {
      -1.0, -1.0, -1.0, 1.0,  0.0,  0.0,  1.0,  1.0,  -1.0, -1.0,
      1.0,  0.0,  0.0,  1.0,  1.0,  1.0,  -1.0, 1.0,  0.0,  0.0,
      1.0,  -1.0, 1.0,  -1.0, 1.0,  0.0,  0.0,  1.0,

      -1.0, -1.0, 1.0,  0.0,  1.0,  0.0,  1.0,  1.0,  -1.0, 1.0,
      0.0,  1.0,  0.0,  1.0,  1.0,  1.0,  1.0,  0.0,  1.0,  0.0,
      1.0,  -1.0, 1.0,  1.0,  0.0,  1.0,  0.0,  1.0,

      -1.0, -1.0, -1.0, 0.0,  0.0,  1.0,  1.0,  -1.0, 1.0,  -1.0,
      0.0,  0.0,  1.0,  1.0,  -1.0, 1.0,  1.0,  0.0,  0.0,  1.0,
      1.0,  -1.0, -1.0, 1.0,  0.0,  0.0,  1.0,  1.0,

      1.0,  -1.0, -1.0, 1.0,  0.5,  0.0,  1.0,  1.0,  1.0,  -1.0,
      1.0,  0.5,  0.0,  1.0,  1.0,  1.0,  1.0,  1.0,  0.5,  0.0,
      1.0,  1.0,  -1.0, 1.0,  1.0,  0.5,  0.0,  1.0,

      -1.0, -1.0, -1.0, 0.0,  0.5,  1.0,  1.0,  -1.0, -1.0, 1.0,
      0.0,  0.5,  1.0,  1.0,  1.0,  -1.0, 1.0,  0.0,  0.5,  1.0,
      1.0,  1.0,  -1.0, -1.0, 0.0,  0.5,  1.0,  1.0,

      -1.0, 1.0,  -1.0, 1.0,  0.0,  0.5,  1.0,  -1.0, 1.0,  1.0,
      1.0,  0.0,  0.5,  1.0,  1.0,  1.0,  1.0,  1.0,  0.0,  0.5,
      1.0,  1.0,  1.0,  -1.0, 1.0,  0.0,  0.5,  1.0};
  sg_buffer vbuf = sg_make_buffer(
      &(sg_buffer_desc){.data = SG_RANGE(vertices), .label = "cube-vertices"});

  /* create an index buffer for the cube */
  uint16_t indices[] = {0,  1,  2,  0,  2,  3,  6,  5,  4,  7,  6,  4,
                        8,  9,  10, 8,  10, 11, 14, 13, 12, 15, 14, 12,
                        16, 17, 18, 16, 18, 19, 22, 21, 20, 23, 22, 20};
  sg_buffer ibuf =
      sg_make_buffer(&(sg_buffer_desc){.type = SG_BUFFERTYPE_INDEXBUFFER,
                                       .data = SG_RANGE(indices),
                                       .label = "cube-indices"});

  /* create shader */
  sg_shader shd = sg_make_shader(cube_shader_desc(sg_query_backend()));

  /* create pipeline object */
  state.pip = sg_make_pipeline(&(sg_pipeline_desc){
      .layout =
          {/* test to provide buffer stride, but no attr offsets */
           .buffers[0].stride = 28,
           .attrs = {[ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT3,
                     [ATTR_vs_color0].format = SG_VERTEXFORMAT_FLOAT4}},
      .shader = shd,
      .index_type = SG_INDEXTYPE_UINT16,
      .cull_mode = SG_CULLMODE_BACK,
      .depth =
          {
              .write_enabled = true,
              .compare = SG_COMPAREFUNC_LESS_EQUAL,
          },
      .label = "cube-pipeline"});

  /* setup resource bindings */
  state.bind = (sg_bindings){.vertex_buffers[0] = vbuf, .index_buffer = ibuf};

  camera_set_up(&state.cam, HMM_Vec3(0.0f, 0.5f, 3.0f));

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

  uint64_t currTime = stm_now();

  float deltaTime = (float)stm_sec(stm_diff(currTime, state.lastFrameTime));
  state.lastFrameTime = currTime;

  const int w = sapp_width();
  const int h = sapp_height();

  const float aspect = (float)w / (float)h;

  camera_update(&state.cam, deltaTime);

  // sdtx_canvas(w * 0.5f, h * 0.5f);
  // sdtx_origin(2, 2);
  // sdtx_puts("Sokol Header Allocations:\n\n");
  // sdtx_printf("  Num: %d\n", smemtrack_info().num_allocs);
  // sdtx_printf("  Allocs: %d bytes\n", smemtrack_info().num_bytes);

  hmm_mat4 view = camera_get_view_matrix(&state.cam);
  hmm_mat4 projection =
      HMM_Perspective(camera_get_fov(&state.cam), aspect, 0.1f, 100.0f);
  vs_params_t vs_params;
  vs_params.mvp =
      HMM_MultiplyMat4(HMM_MultiplyMat4(projection, view), HMM_Mat4d(1.0f));

  sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
  sg_apply_pipeline(state.pip);
  sg_apply_bindings(&state.bind);
  sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
  sg_draw(0, 36, 1);
  // sdtx_draw();
  __cdbgui_draw();
  sg_end_pass();
  sg_commit();
}

void event(const sapp_event* e) {
  __cdbgui_event(e);
  if (e->type == SAPP_EVENTTYPE_KEY_DOWN) {
    if (e->key_code == SAPP_KEYCODE_ESCAPE) {
      sapp_request_quit();
    }
  }
  hmm_vec2 mouse_offset = HMM_Vec2(0.0f, 0.0f);
  if (e->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
    if (!state.first_mouse) {
      mouse_offset.X = e->mouse_x - state.last_mouse.X;
      mouse_offset.Y = state.last_mouse.Y - e->mouse_y;
    } else {
      state.first_mouse = false;
    }
    state.last_mouse.X = e->mouse_x;
    state.last_mouse.Y = e->mouse_y;
  }
  camera_handle_input(&state.cam, e, mouse_offset);
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
