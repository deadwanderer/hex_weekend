#define _CRT_SECURE_NO_WARNINGS (1)
//------------------------------------------------------------------------------
//  clear-sapp.c
//------------------------------------------------------------------------------
#include "sokol_memtrack.h"
#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_time.h"
#include "sokol_debugtext.h"
#include "sokol_fetch.h"
#include "sokol_shape.h"
#include "sokol_glue.h"
#include "cdbgui/cdbgui.h"
#include "HandmadeMath/HandmadeMath.h"
#include "shaders.glsl.h"

#include "config.h"

#include "stb/stb_image.h"

#include "Camera.h"
// #include "hex.h"

static uint8_t favicon_buffer[32 * 32 * 4];

typedef void (*fail_callback_t)();

typedef struct image_request_t {
  const char* path;
  sg_image img_id;
  sg_wrap wrap_u;
  sg_wrap wrap_v;
  void* buffer_ptr;
  uint32_t buffer_size;
  fail_callback_t fail_callback;
} image_request_t;

typedef struct cubemap_request_t {
  const char* path_right;
  const char* path_left;
  const char* path_up;
  const char* path_down;
  const char* path_front;
  const char* path_back;
  sg_image img_id;
  uint8_t* buffer_ptr;
  uint32_t buffer_offset;
  fail_callback_t fail_callback;
} cubemap_request_t;

typedef struct _cubemap_request_t {
  sg_image img_id;
  uint8_t* buffer;
  int buffer_offset;
  int fetched_sizes[6];
  int finished_requests;
  bool failed;
  fail_callback_t fail_callback;
} _cubemap_request_t;

typedef struct _cubemap_request_instance_t {
  int index;
  _cubemap_request_t* request;
} _cubemap_request_instance_t;

typedef struct {
  sg_image img_id;
  sg_wrap wrap_u;
  sg_wrap wrap_v;
  const char* label;
  fail_callback_t fail_callback;
} image_request_data;

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
  sg_pipeline cube_pip;
  sg_bindings cube_bind;
  sg_pipeline skybox_pip;
  sg_bindings skybox_bind;
  sg_pipeline shape_pip;
  sg_bindings shape_bind;
  sg_pass_action pass_action;
  sshape_element_range_t shape_elems;
  _cubemap_request_t cubemap_req;
  uint8_t texture_buffer[1024 * 1024];
  uint8_t cubemap_buffer[6 * 1024 * 1024];
  camera_t cam;
  hmm_vec2 last_mouse;
  bool first_mouse;
  bool show_debug_ui;
  bool show_mem_ui;
  uint64_t lastFrameTime;
} state;

const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 768;
const float VELOCITY = 25.0f;

static void fail_callback() {
  state.pass_action = (sg_pass_action){
      .colors[0] = {.action = SG_ACTION_CLEAR,
                    .value = (sg_color){1.0f, 0.0f, 0.0f, 1.0f}}};
}

static void icon_fetch_callback(const sfetch_response_t* response) {
  if (response->fetched) {
    int png_width, png_height, num_channels;
    const int desired_channels = 4;

    stbi_uc* pixels = stbi_load_from_memory(
        response->buffer_ptr, (int)response->fetched_size, &png_width,
        &png_height, &num_channels, desired_channels);
    if (pixels) {
      sapp_set_icon(&(sapp_icon_desc){
          .images = {
              {.width = 32,
               .height = 32,
               .pixels = {.ptr = pixels,
                          .size = (size_t)(png_width * png_height * 4)}}}});
    }
  }
}

static void image_fetch_callback(const sfetch_response_t* response) {
  image_request_data req_data = *(image_request_data*)response->user_data;

  if (response->fetched) {
    int img_width, img_height, num_channels;
    const int desired_channels = 4;
    stbi_uc* pixels = stbi_load_from_memory(
        response->buffer_ptr, (int)response->fetched_size, &img_width,
        &img_height, &num_channels, desired_channels);
    if (pixels) {
      sg_init_image(req_data.img_id,
                    &(sg_image_desc){.width = img_width,
                                     .height = img_height,
                                     .pixel_format = SG_PIXELFORMAT_RGBA8,
                                     .wrap_u = req_data.wrap_u,
                                     .wrap_v = req_data.wrap_v,
                                     .min_filter = SG_FILTER_LINEAR,
                                     .mag_filter = SG_FILTER_LINEAR,
                                     .data.subimage[0][0] =
                                         {
                                             .ptr = pixels,
                                             .size = img_width * img_height *
                                                     desired_channels,
                                         },
                                     .label = req_data.label});
      stbi_image_free(pixels);
    }
  } else if (response->failed) {
    req_data.fail_callback();
  }
}

void load_image(const image_request_t* request, const char* image_label) {
  image_request_data req_data = {.img_id = request->img_id,
                                 .wrap_u = request->wrap_u,
                                 .wrap_v = request->wrap_v,
                                 .fail_callback = request->fail_callback,
                                 .label = image_label};

  sfetch_send(&(sfetch_request_t){.path = request->path,
                                  .callback = image_fetch_callback,
                                  .buffer_ptr = request->buffer_ptr,
                                  .buffer_size = request->buffer_size,
                                  .user_data_ptr = &req_data,
                                  .user_data_size = sizeof(req_data)});
}

static bool _load_cubemap(_cubemap_request_t* request) {
  const int desired_channels = 4;
  int img_widths[6], img_heights[6];
  stbi_uc* pixels_ptrs[6];
  sg_image_data img_data;

  for (int i = 0; i < 6; i++) {
    int num_channel;
    pixels_ptrs[i] =
        stbi_load_from_memory(request->buffer + (i * request->buffer_offset),
                              request->fetched_sizes[i], &img_widths[i],
                              &img_heights[i], &num_channel, desired_channels);
    img_data.subimage[i][0].ptr = pixels_ptrs[i];
    img_data.subimage[i][0].size =
        img_widths[i] * img_heights[i] * desired_channels;
  }

  bool valid = img_heights[0] > 0 && img_widths[0] > 0;

  for (int i = 1; i < 6; i++) {
    if (img_widths[i] != img_widths[0] || img_heights[i] != img_heights[0]) {
      valid = false;
      break;
    }
  }

  if (valid) {
    sg_init_image(request->img_id,
                  &(sg_image_desc){.type = SG_IMAGETYPE_CUBE,
                                   .width = img_widths[0],
                                   .height = img_heights[0],
                                   .pixel_format = SG_PIXELFORMAT_RGBA8,
                                   .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
                                   .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
                                   .wrap_w = SG_WRAP_CLAMP_TO_EDGE,
                                   .min_filter = SG_FILTER_LINEAR,
                                   .mag_filter = SG_FILTER_LINEAR,
                                   .data = img_data,
                                   .label = "cubemap-image"});
  }

  for (int i = 0; i < 6; i++) {
    stbi_image_free(pixels_ptrs[i]);
  }

  return valid;
}

static void cubemap_fetch_callback(const sfetch_response_t* response) {
  _cubemap_request_instance_t req_inst =
      *(_cubemap_request_instance_t*)response->user_data;
  _cubemap_request_t* request = req_inst.request;

  if (response->fetched) {
    request->fetched_sizes[req_inst.index] = response->fetched_size;
    ++request->finished_requests;
  } else if (response->failed) {
    request->failed = true;
    ++request->finished_requests;
  }

  if (request->finished_requests == 6) {
    if (!request->failed) {
      request->failed = !_load_cubemap(request);
    }

    if (request->failed) {
      request->fail_callback();
    }
  }
}

void load_cubemap(cubemap_request_t* request) {
  state.cubemap_req =
      (_cubemap_request_t){.img_id = request->img_id,
                           .buffer = request->buffer_ptr,
                           .buffer_offset = request->buffer_offset,
                           .fail_callback = request->fail_callback};

  const char* cubemap[6] = {request->path_right, request->path_left,
                            request->path_up,    request->path_down,
                            request->path_front, request->path_back};

  for (int i = 0; i < 6; ++i) {
    _cubemap_request_instance_t req_instance = {.index = i,
                                                .request = &state.cubemap_req};
    sfetch_send(&(sfetch_request_t){
        .path = cubemap[i],
        .callback = cubemap_fetch_callback,
        .buffer_ptr = request->buffer_ptr + (i * request->buffer_offset),
        .buffer_size = request->buffer_offset,
        .user_data_ptr = &req_instance,
        .user_data_size = sizeof(req_instance)});
  }
}

void init(void) {
  sg_setup(&(sg_desc){.context = sapp_sgcontext()});
  sfetch_setup(
      &(sfetch_desc_t){.max_requests = 8, .num_channels = 2, .num_lanes = 4});
  stm_setup();
  state.show_debug_ui = false;
  state.show_mem_ui = false;
  state.lastFrameTime = stm_now();
  state.pass_action =
      (sg_pass_action){.colors[0] = {.action = SG_ACTION_CLEAR,
                                     .value = {0.1f, 0.15f, 0.2f, 1.0f}}};
  sdtx_setup(&(sdtx_desc_t){
      .fonts[0] = sdtx_font_oric(),
  });
  __cdbgui_setup(sapp_sample_count());

  float vertices[] = {
      // back
      // position          // texcoord
      -1.0f, -1.0f, -1.0f, 1.0f, 1.0f,  // 1.0,  0.0,  0.0,  1.0,
      1.0f, -1.0f, -1.0f, 0.0f, 1.0f,   // 1.0,  0.0,  0.0,  1.0,
      1.0f, 1.0f, -1.0f, 0.0f, 0.0f,    // 1.0,  0.0,  0.0, 1.0,
      -1.0f, 1.0f, -1.0f, 1.0f, 0.0f,   // 1.0,  0.0,  0.0,  1.0,

      // front
      -1.0f, -1.0f, 1.0f, 0.0f, 1.0f,  // 0.0,  1.0,  0.0,  1.0,
      1.0f, -1.0f, 1.0f, 1.0f, 1.0f,   // 0.0,  1.0,  0.0,  1.0,
      1.0f, 1.0f, 1.0f, 1.0f, 0.0f,    // 0.0,  1.0,  0.0, 1.0,
      -1.0f, 1.0f, 1.0f, 0.0f, 0.0f,   // 0.0,  1.0,  0.0,  1.0,

      // left
      -1.0f, -1.0f, -1.0f, 0.0f, 1.0f,  // 0.0,  0.0,  1.0,  1.0,
      -1.0f, 1.0f, -1.0f, 0.0f, 0.0f,   // 0.0,  0.0,  1.0,  1.0,
      -1.0f, 1.0f, 1.0f, 1.0f, 0.0f,    // 0.0,  0.0,  1.0, 1.0,
      -1.0f, -1.0f, 1.0f, 1.0f, 1.0f,   // 0.0,  0.0,  1.0,  1.0,

      // right
      1.0f, -1.0f, -1.0f, 1.0f, 1.0f,  // 1.0,  0.5,  0.0,  1.0,
      1.0f, 1.0f, -1.0f, 1.0f, 0.0f,   // 1.0,  0.5,  0.0,  1.0,
      1.0f, 1.0f, 1.0f, 0.0f, 0.0f,    // 1.0,  0.5,  0.0, 1.0,
      1.0f, -1.0f, 1.0f, 0.0f, 1.0f,   // 1.0,  0.5,  0.0,  1.0,

      // bottom
      -1.0f, -1.0f, -1.0f, 0.0f, 0.0f,  // 0.0,  0.5,  1.0,  1.0,
      -1.0f, -1.0f, 1.0f, 0.0f, 1.0f,   // 0.0,  0.5,  1.0,  1.0,
      1.0f, -1.0f, 1.0f, 1.0f, 1.0f,    // 0.0,  0.5,  1.0, 1.0,
      1.0f, -1.0f, -1.0f, 1.0f, 0.0f,   // 0.0,  0.5,  1.0,  1.0,

      // top
      -1.0f, 1.0f, -1.0f, 1.0f, 0.0f,  // 1.0,  0.0,  0.5,  1.0,
      -1.0f, 1.0f, 1.0f, 1.0f, 1.0f,   // 1.0,  0.0,  0.5,  1.0,
      1.0f, 1.0f, 1.0f, 0.0f, 1.0f,    // 1.0,  0.0,  0.5, 1.0,
      1.0f, 1.0f, -1.0f, 0.0f, 0.0f,   // 1.0,  0.0,  0.5,  1.0
  };

  sg_buffer vbuf = sg_make_buffer(
      &(sg_buffer_desc){.data = SG_RANGE(vertices), .label = "cube-vertices"});

  /* create an index buffer for the cube */
  uint16_t indices[] = {
      0,  1,  2,  0,  2,  3,   // back
      6,  5,  4,  7,  6,  4,   // front
      10, 11, 8,  9,  10, 8,   // left
      14, 13, 12, 15, 14, 12,  // right
      16, 17, 18, 16, 18, 19,  // bottom
      22, 21, 20, 23, 22, 20   // top
  };
  sg_buffer ibuf =
      sg_make_buffer(&(sg_buffer_desc){.type = SG_BUFFERTYPE_INDEXBUFFER,
                                       .data = SG_RANGE(indices),
                                       .label = "cube-indices"});

  /* create shader */
  sg_shader shd = sg_make_shader(textured_cube_shader_desc(sg_query_backend()));

  /* create pipeline object */
  state.cube_pip = sg_make_pipeline(&(sg_pipeline_desc){
      .layout =
          {/* test to provide buffer stride, but no attr offsets */
           .buffers[0].stride = 20,
           .attrs = {[ATTR_textured_vs_position].format =
                         SG_VERTEXFORMAT_FLOAT3,
                     [ATTR_textured_vs_texcoord0].format =
                         SG_VERTEXFORMAT_FLOAT2}},
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
  state.cube_bind =
      (sg_bindings){.vertex_buffers[0] = vbuf, .index_buffer = ibuf};

  /* Set up skybox */
  float skybox_vertices[] = {
      // positions
      -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
      1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,

      -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
      -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,

      1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,
      1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,

      -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
      1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

      -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,
      1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,

      -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f,
      1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f};

  sg_buffer skybox_buffer = sg_make_buffer(&(sg_buffer_desc){
      .data = SG_RANGE(skybox_vertices), .label = "skybox-vertices"});

  state.skybox_bind.vertex_buffers[0] = skybox_buffer;
  sg_image skybox_img_id = sg_alloc_image();
  state.skybox_bind.fs_images[SLOT_skybox_texture] = skybox_img_id;
  sg_image cube_img_id = sg_alloc_image();
  state.cube_bind.fs_images[SLOT_cube_texture] = cube_img_id;

  state.skybox_pip = sg_make_pipeline(&(sg_pipeline_desc){
      .shader = sg_make_shader(skybox_shader_desc(sg_query_backend())),
      .layout =
          {
              .attrs =
                  {
                      [ATTR_vs_skybox_a_pos].format = SG_VERTEXFORMAT_FLOAT3,
                  },
          },
      .depth =
          {
              .compare = SG_COMPAREFUNC_LESS_EQUAL,
          },
      .label = "skybox-pipeline"});

  // SHAPES
  state.shape_pip = sg_make_pipeline(&(sg_pipeline_desc){
      .shader = sg_make_shader(shape_shader_desc(sg_query_backend())),
      .layout = {.buffers[0] = sshape_buffer_layout_desc(),
                 .attrs = {[0] = sshape_position_attr_desc(),
                           [1] = sshape_normal_attr_desc(),
                           [2] = sshape_texcoord_attr_desc(),
                           [3] = sshape_color_attr_desc()}},
      .index_type = SG_INDEXTYPE_UINT16,
      .cull_mode = SG_CULLMODE_NONE,
      .depth = {.compare = SG_COMPAREFUNC_LESS_EQUAL, .write_enabled = true},
      .label = "shape-pipeline",
  });

  sshape_vertex_t shape_vertices[6 * 1024];
  uint16_t shape_indices[16 * 1024];
  sshape_buffer_t buf = {
      .vertices.buffer = SSHAPE_RANGE(shape_vertices),
      .indices.buffer = SSHAPE_RANGE(shape_indices),
  };

  // const hmm_mat4 box_transform = HMM_Translate(HMM_Vec3(-2.0f, 0.0f, -2.0f));
  // const hmm_mat4 sphere_transform = HMM_Translate(HMM_Vec3(2.0f, 0.0f,
  // -2.0f));
  // const hmm_mat4 cylinder_transform =
  //     HMM_Translate(HMM_Vec3(2.0f, 0.0f, -4.0f));
  // const hmm_mat4 torus_transform = HMM_Translate(HMM_Vec3(-2.0f, 0.0f,
  // -4.0f));

  // buf = sshape_build_box(
  //     &buf,
  //     &(sshape_box_t){.width = 2.0f,
  //                     .height = 2.0f,
  //                     .depth = 2.0f,
  //                     .tiles = 1,
  //                     .random_colors = true,
  //                     .transform =
  //                     sshape_mat4(&box_transform.Elements[0][0])});
  // buf = sshape_build_sphere(
  //     &buf, &(sshape_sphere_t){
  //               .merge = true,
  //               .radius = 0.75f,
  //               .slices = 36,
  //               .stacks = 20,
  //               .random_colors = true,
  //               .transform = sshape_mat4(&sphere_transform.Elements[0][0])});
  buf = sshape_build_cylinder(
      &buf, &(sshape_cylinder_t){
                .merge = true,
                .radius = 1.0f,
                .height = 1.0f,
                .slices = 6,
                .stacks = 1,
                .random_colors = true,
                // .transform = sshape_mat4(&cylinder_transform.Elements[0][0])
            });
  // buf = sshape_build_torus(
  //     &buf, &(sshape_torus_t){
  //               .merge = true,
  //               .radius = 0.75f,
  //               .ring_radius = 0.3f,
  //               .rings = 36,
  //               .sides = 18,
  //               .random_colors = true,
  //               .transform = sshape_mat4(&torus_transform.Elements[0][0])});

  state.shape_elems = sshape_element_range(&buf);
  sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&buf);
  vbuf_desc.label = "shape-vertices";
  sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&buf);
  ibuf_desc.label = "shape-indices";
  state.shape_bind.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);
  state.shape_bind.index_buffer = sg_make_buffer(&ibuf_desc);

  camera_set_up(&state.cam, HMM_Vec3(0.0f, 1.5f, 3.0f));

  sfetch_send(&(sfetch_request_t){.path = "favicon-32x32.png",
                                  .callback = icon_fetch_callback,
                                  .buffer_ptr = favicon_buffer,
                                  .buffer_size = sizeof(favicon_buffer)});

  load_image(&(image_request_t){.img_id = cube_img_id,
                                .path = "container2.png",
                                .buffer_ptr = state.texture_buffer,
                                .buffer_size = sizeof(state.texture_buffer),
                                .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
                                .wrap_v = SG_WRAP_CLAMP_TO_EDGE},
             "cube-image");

  load_cubemap(&(cubemap_request_t){.img_id = skybox_img_id,
                                    .path_right = "right.jpg",
                                    .path_left = "left.jpg",
                                    .path_up = "up.jpg",
                                    .path_down = "down.jpg",
                                    .path_front = "front.jpg",
                                    .path_back = "back.jpg",
                                    .buffer_ptr = state.cubemap_buffer,
                                    .buffer_offset = 1024 * 1024,
                                    .fail_callback = fail_callback});
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

  if (state.show_mem_ui) {
    sdtx_canvas(w * 0.5f, h * 0.5f);
    sdtx_origin(2, 2);
    sdtx_puts("Sokol Header Allocations:\n\n");
    sdtx_printf("  Num: %d\n", smemtrack_info().num_allocs);
    sdtx_printf("  Allocs: %d bytes\n", smemtrack_info().num_bytes);
  }

  hmm_mat4 view = camera_get_view_matrix(&state.cam);
  hmm_mat4 projection =
      HMM_Perspective(camera_get_fov(&state.cam), aspect, 0.1f, 100.0f);

  // DRAW CUBE
  // vs_params_t vs_params;
  // vs_params.mvp =
  // HMM_MultiplyMat4(HMM_MultiplyMat4(projection, view), HMM_Mat4d(1.0f));

  sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
  // sg_apply_pipeline(state.cube_pip);
  // sg_apply_bindings(&state.cube_bind);
  // sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params, &SG_RANGE(vs_params));
  // sg_draw(0, 36, 1);

  // DRAW SHAPES
  shape_vs_params_t shape_params;
  shape_params.draw_mode = 0.0f;
  sg_apply_pipeline(state.shape_pip);
  sg_apply_bindings(&state.shape_bind);
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      float x = ((float)i + j * 0.5f - j / 2) * (2.0f * 0.866025404f);
      float z = (float)j * 1.5f;

      hmm_mat4 model = HMM_Translate(HMM_Vec3(x - 2, 0.0f, -z));
      shape_params.mvp =
          HMM_MultiplyMat4(HMM_MultiplyMat4(projection, view), model);

      sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_shape_vs_params,
                        &SG_RANGE(shape_params));
      sg_draw(state.shape_elems.base_element, state.shape_elems.num_elements,
              1);
    }
  }

  // DRAW SKYBOX
  view.Elements[3][0] = 0.0f;
  view.Elements[3][1] = 0.0f;
  view.Elements[3][2] = 0.0f;

  skybox_vs_params_t skybox_params;
  skybox_params.view = view;
  skybox_params.projection = projection;

  sg_apply_pipeline(state.skybox_pip);
  sg_apply_bindings(&state.skybox_bind);
  sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_skybox_vs_params,
                    &SG_RANGE(skybox_params));
  sg_draw(0, 36, 1);

  if (state.show_mem_ui) {
    sdtx_draw();
  }
  if (state.show_debug_ui) {
    __cdbgui_draw();
  }
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
  if (e->type == SAPP_EVENTTYPE_KEY_UP) {
    if (e->key_code == SAPP_KEYCODE_H) {
      state.show_debug_ui = !state.show_debug_ui;
    }
    if (e->key_code == SAPP_KEYCODE_N) {
      state.show_mem_ui = !state.show_mem_ui;
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
  char app_title[21];
  sprintf(app_title, "App Version %s.%s.%s", PROJECT_VERSION_MAJOR,
          PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH);
  app_title[20] = '\0';
  return (sapp_desc){
      .init_cb = init,
      .frame_cb = frame,
      .cleanup_cb = cleanup,
      .event_cb = event,
      .width = SCREEN_WIDTH,
      .height = SCREEN_HEIGHT,
      .gl_force_gles2 = false,
      .window_title = app_title,
      .icon.sokol_default = false,
  };
}
