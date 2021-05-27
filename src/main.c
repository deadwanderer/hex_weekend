#define _CRT_SECURE_NO_WARNINGS (1)

#include <stdlib.h>
#include <time.h>

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
#include "types.h"
#include "asset_loading.h"

#include "stb/stb_image.h"

#include "Camera.h"
// #include "hex.h"

#define NUM_CELLS_WIDE 100
#define NUM_CELLS_LONG 100
#define NUM_CELLS (NUM_CELLS_WIDE * NUM_CELLS_LONG)

static uint8_t favicon_buffer[32 * 32 * 4];

static struct {
  // sg_pipeline cube_pip;
  // sg_bindings cube_bind;
  sg_pipeline skybox_pip;
  sg_bindings skybox_bind;
  sg_pipeline shape_pip;
  sg_bindings shape_bind;
  sg_pass_action pass_action;
  sshape_element_range_t shape_elems;
  _cubemap_request_t cubemap_req;
  _arraytex_request_t arraytex_req;
  // uint8_t texture_buffer[1024 * 1024];
  uint8_t cubemap_buffer[6 * 1024 * 1024];
  uint8_t shape_texture_buffer[6 * 256 * 1024];
  uint8_t arraytex_load_buffer[ARRAYTEX_IMAGE_BUFFER_SIZE];
  uint32_t arraytex_buffer[ARRAYTEX_IMAGE_BUFFER_SIZE];
  camera_t cam;
  hmm_vec2 last_mouse;
  bool first_mouse;
  bool show_debug_ui;
  bool show_mem_ui;
  uint64_t lastFrameTime;
  uint64_t timeToLoadCubemap;
  uint64_t timeToLoadArrayTextures;
  uint64_t imageLoadStartTime;
  uint64_t renderTime;
  uint64_t initTime;
} state;

static void fail_callback() {
  state.pass_action = (sg_pass_action){
      .colors[0] = {.action = SG_ACTION_CLEAR,
                    .value = (sg_color){1.0f, 0.0f, 0.0f, 1.0f}}};
}

static void cube_success_callback() {
  state.timeToLoadCubemap = stm_diff(stm_now(), state.imageLoadStartTime);
}
static void arraytex_success_callback() {
  state.timeToLoadArrayTextures = stm_diff(stm_now(), state.imageLoadStartTime);
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

void load_array_texture(arraytex_request_t* request) {
  state.arraytex_req =
      (_arraytex_request_t){.img_id = request->img_id,
                            .buffer = request->buffer_ptr,
                            .texture_buffer_ptr = request->texture_buffer_ptr,
                            .buffer_offset = request->buffer_offset,
                            .fail_callback = request->fail_callback,
                            .success_callback = request->success_callback};

  for (int i = 0; i < ARRAYTEX_COUNT; ++i) {
    _arraytex_request_instance_t req_inst = {.index = i,
                                             .request = &state.arraytex_req};
    sfetch_send(&(sfetch_request_t){
        .path = request->paths[i],
        .callback = arraytex_fetch_callback,
        .buffer_ptr = request->buffer_ptr + (i * request->buffer_offset),
        .buffer_size = request->buffer_offset,
        .user_data_ptr = &req_inst,
        .user_data_size = sizeof(req_inst)});
  }
}

void load_cubemap(cubemap_request_t* request) {
  state.cubemap_req =
      (_cubemap_request_t){.img_id = request->img_id,
                           .buffer = request->buffer_ptr,
                           .buffer_offset = request->buffer_offset,
                           .fail_callback = request->fail_callback,
                           .success_callback = request->success_callback};

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
      &(sfetch_desc_t){.max_requests = 16, .num_channels = 4, .num_lanes = 4});
  stm_setup();
  uint64_t initStartTime = stm_now();
  state.show_debug_ui = false;
  state.show_mem_ui = false;
  state.lastFrameTime = stm_now();
  state.renderTime = 0;
  state.initTime = 0;
  state.imageLoadStartTime = 0;
  state.timeToLoadCubemap = 0;
  state.timeToLoadArrayTextures = 0;
  state.pass_action =
      (sg_pass_action){.colors[0] = {.action = SG_ACTION_CLEAR,
                                     .value = {0.1f, 0.15f, 0.2f, 1.0f}}};
  sdtx_setup(&(sdtx_desc_t){
      .fonts[0] = sdtx_font_oric(),
  });
  __cdbgui_setup(sapp_sample_count());

  /*
    float vertices[] = {
        -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f,  -1.0f, -1.0f, 0.0f, 1.0f,
        1.0f,  1.0f,  -1.0f, 0.0f, 0.0f, -1.0f, 1.0f,  -1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 1.0f,  0.0f, 1.0f, 1.0f,  -1.0f, 1.0f,  1.0f, 1.0f,
        1.0f,  1.0f,  1.0f,  1.0f, 0.0f, -1.0f, 1.0f,  1.0f,  0.0f, 0.0f,
        -1.0f, -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 1.0f,  -1.0f, 0.0f, 0.0f,
        -1.0f, 1.0f,  1.0f,  1.0f, 0.0f, -1.0f, -1.0f, 1.0f,  1.0f, 1.0f,
        1.0f,  -1.0f, -1.0f, 1.0f, 1.0f, 1.0f,  1.0f,  -1.0f, 1.0f, 0.0f,
        1.0f,  1.0f,  1.0f,  0.0f, 0.0f, 1.0f,  -1.0f, 1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, -1.0f, 1.0f,  0.0f, 1.0f,
        1.0f,  -1.0f, 1.0f,  1.0f, 1.0f, 1.0f,  -1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f, 1.0f,  -1.0f, 1.0f, 0.0f, -1.0f, 1.0f,  1.0f,  1.0f, 1.0f,
        1.0f,  1.0f,  1.0f,  0.0f, 1.0f, 1.0f,  1.0f,  -1.0f, 0.0f, 0.0f,
    };

    sg_buffer vbuf = sg_make_buffer(
        &(sg_buffer_desc){.data = SG_RANGE(vertices), .label =
    "cube-vertices"});

  uint16_t indices[] = {0,  1,  2,  0,  2,  3,  6,  5,  4,  7,  6,  4,
                        10, 11, 8,  9,  10, 8,  14, 13, 12, 15, 14, 12,
                        16, 17, 18, 16, 18, 19, 22, 21, 20, 23, 22, 20};
  sg_buffer ibuf =
      sg_make_buffer(&(sg_buffer_desc){.type = SG_BUFFERTYPE_INDEXBUFFER,
                                       .data = SG_RANGE(indices),
                                       .label = "cube-indices"});

  sg_shader shd = sg_make_shader(textured_cube_shader_desc(sg_query_backend()));

  state.cube_pip = sg_make_pipeline(&(sg_pipeline_desc){
      .layout =
          {.buffers[0].stride = 20,
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

  state.cube_bind =
      (sg_bindings){.vertex_buffers[0] = vbuf, .index_buffer = ibuf};
  */

  // Set Up Skybox
  float skybox_vertices[] = {
      -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,
      -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f,
      1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,
      -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,
      -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
      -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,
      1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,
      -1.0f, 1.0f,  -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,
      1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,
      -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,
      -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f};

  sg_buffer skybox_buffer = sg_make_buffer(&(sg_buffer_desc){
      .data = SG_RANGE(skybox_vertices), .label = "skybox-vertices"});

  state.skybox_bind.vertex_buffers[0] = skybox_buffer;
  sg_image skybox_img_id = sg_alloc_image();
  state.skybox_bind.fs_images[SLOT_skybox_texture] = skybox_img_id;
  // sg_image cube_img_id = sg_alloc_image();
  // state.cube_bind.fs_images[SLOT_cube_texture] = cube_img_id;
  sg_image shape_img_id = sg_alloc_image();
  state.shape_bind.fs_images[SLOT_shape_texture] = shape_img_id;
  sg_image arraytex_img_id = sg_alloc_image();
  state.shape_bind.fs_images[SLOT_shape_arraytex] = arraytex_img_id;

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

  hmm_vec3 shape_pos[NUM_CELLS];
  srand((unsigned int)time(NULL));
  for (int z = 0; z < NUM_CELLS_LONG; ++z) {
    for (int x = 0; x < NUM_CELLS_WIDE; ++x) {
      float y = (float)(rand() % (11) - 5) / 25.0f;
      shape_pos[NUM_CELLS_WIDE * z + x] =
          HMM_Vec3((float)(x - 50), y, (float)(z - 50));
    }
  }

  state.shape_pip = sg_make_pipeline(&(sg_pipeline_desc){
      .shader = sg_make_shader(textured_shape_shader_desc(sg_query_backend())),
      .layout = {.buffers[0] = sshape_buffer_layout_desc(),
                 .buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE,
                 .attrs = {[0] = sshape_position_attr_desc(),
                           [1] = sshape_normal_attr_desc(),
                           [2] = sshape_texcoord_attr_desc(),
                           [3] = sshape_color_attr_desc(),
                           [4] = {.format = SG_VERTEXFORMAT_FLOAT3,
                                  .buffer_index = 1}}},
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

  buf = sshape_build_cylinder(&buf, &(sshape_cylinder_t){
                                        .merge = true,
                                        .radius = 1.0f,
                                        .height = 1.0f,
                                        .slices = 6,
                                        .stacks = 1,
                                        .random_colors = true,
                                    });

  state.shape_elems = sshape_element_range(&buf);
  sg_buffer_desc vbuf_desc = sshape_vertex_buffer_desc(&buf);
  vbuf_desc.label = "shape-vertices";
  sg_buffer_desc ibuf_desc = sshape_index_buffer_desc(&buf);
  ibuf_desc.label = "shape-indices";
  sg_buffer_desc instbuf_desc =
      (sg_buffer_desc){.data = SG_RANGE(shape_pos), .label = "instance-data"};
  state.shape_bind.vertex_buffers[0] = sg_make_buffer(&vbuf_desc);
  state.shape_bind.vertex_buffers[1] = sg_make_buffer(&instbuf_desc);
  state.shape_bind.index_buffer = sg_make_buffer(&ibuf_desc);

  camera_set_up(&state.cam, HMM_Vec3(0.0f, 2.5f, 2.0f),
                (&(cam_desc_t){
                    // .constrain_movement = true,
                    .min_x = -3.5f,
                    .max_x = 7.5f,
                    .min_y = 1.01f,
                    .max_y = 5.0f,
                    .min_z = -7.5f,
                    .max_z = 3.5f,
                }));

  state.imageLoadStartTime = stm_now();
  sfetch_send(&(sfetch_request_t){.path = "favicon-32x32.png",
                                  .callback = icon_fetch_callback,
                                  .buffer_ptr = favicon_buffer,
                                  .buffer_size = sizeof(favicon_buffer)});

  // load_image(&(image_request_t){.img_id = cube_img_id,
  //                               .path = "container2.png",
  //                               .buffer_ptr = state.texture_buffer,
  //                               .buffer_size = sizeof(state.texture_buffer),
  //                               .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
  //                               .wrap_v = SG_WRAP_CLAMP_TO_EDGE},
  //            "cube-image");

  load_image(
      &(image_request_t){.img_id = shape_img_id,
                         .path = "sand.png",
                         .buffer_ptr = state.shape_texture_buffer,
                         .buffer_size = sizeof(state.shape_texture_buffer),
                         .wrap_u = SG_WRAP_REPEAT,
                         .wrap_v = SG_WRAP_REPEAT},
      "shape-texture");

  const char* arraytex_paths[ARRAYTEX_COUNT] = {
      "grass.png", "mud.png", "rock.png", "sand.png", "snow.png", "stone.png",
  };

  load_array_texture(&(arraytex_request_t){
      .img_id = arraytex_img_id,
      .paths = arraytex_paths,
      .image_count = ARRAYTEX_COUNT,
      .buffer_ptr = state.arraytex_load_buffer,
      .texture_buffer_ptr = (uint8_t*)state.arraytex_buffer,
      .buffer_offset = ARRAYTEX_ARRAY_IMAGE_OFFSET,
      .fail_callback = fail_callback,
      .success_callback = arraytex_success_callback});

  load_cubemap(&(cubemap_request_t){.img_id = skybox_img_id,
                                    .path_right = "right.jpg",
                                    .path_left = "left.jpg",
                                    .path_up = "up.jpg",
                                    .path_down = "down.jpg",
                                    .path_front = "front.jpg",
                                    .path_back = "back.jpg",
                                    .buffer_ptr = state.cubemap_buffer,
                                    .buffer_offset = 1024 * 1024,
                                    .fail_callback = fail_callback,
                                    .success_callback = cube_success_callback});
  state.initTime = stm_diff(stm_now(), initStartTime);
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

  sdtx_canvas(w * 0.5f, h * 0.5f);
  // sdtx_canvas(w, h);
  // sdtx_origin(2, 2);
  sdtx_origin(1, 1);
  sdtx_color3f(1.0f, 0.2f, 0.2f);
  sdtx_printf("CamPos: (%.2f, %.2f, %.2f)\n", state.cam.position.X,
              state.cam.position.Y, state.cam.position.Z);

  if (state.initTime > 0) {
    sdtx_move_y(1);
    sdtx_printf("Init Time: %.2f\n", (float)stm_ms(state.initTime));
  }
  if (state.timeToLoadCubemap > 0) {
    sdtx_move_y(1);
    sdtx_printf("Cubemap Load Time: %.2f\n",
                (float)stm_ms(state.timeToLoadCubemap));
  }
  if (state.timeToLoadArrayTextures > 0) {
    sdtx_move_y(1);
    sdtx_printf("Arraytex Load Time: %.2f\n",
                (float)stm_ms(state.timeToLoadArrayTextures));
  }
  if (state.renderTime > 0) {
    sdtx_move_y(1);
    sdtx_printf("Render Time: %.2f\n", (float)stm_ms(state.renderTime));
  }
  sdtx_move_y(2);
  sdtx_printf("Frame Time: %.2f (%d FPS)\n",
              (float)stm_ms(stm_diff(currTime, state.lastFrameTime)),
              (int)(1 / deltaTime));

  if (state.show_mem_ui) {
    sdtx_move_y(2);
    sdtx_puts("Sokol Header Allocations:\n\n");
    sdtx_printf("  Num: %d\n", smemtrack_info().num_allocs);
    sdtx_printf("  Allocs: %d bytes\n", smemtrack_info().num_bytes);
  }

  hmm_mat4 view = camera_get_view_matrix(&state.cam);
  hmm_mat4 projection =
      HMM_Perspective(camera_get_fov(&state.cam), aspect, 0.1f, 1000.0f);

  uint64_t renderStartTime = stm_now();
  sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());

  // DRAW CUBE
  // vs_params_t vs_params;
  // vs_params.mvp =
  // HMM_MultiplyMat4(HMM_MultiplyMat4(projection, view), HMM_Mat4d(1.0f));

  // sg_apply_pipeline(state.cube_pip);
  // sg_apply_bindings(&state.cube_bind);
  // sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_vs_params,
  // &SG_RANGE(vs_params)); sg_draw(0, 36, 1);

  // DRAW SHAPES
  textured_shape_vs_params_t shape_params;
  sg_apply_pipeline(state.shape_pip);
  sg_apply_bindings(&state.shape_bind);
  shape_params.viewproj = HMM_MultiplyMat4(projection, view);
  shape_params.model = HMM_Mat4d(1.0);
  sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_shape_vs_params,
                    &SG_RANGE(shape_params));
  sg_draw(state.shape_elems.base_element, state.shape_elems.num_elements,
          NUM_CELLS);

  shape_params.model = HMM_Translate(HMM_Vec3(x - 2, 0.0f, -z));
  int iZ = abs((int)z % ARRAYTEX_COUNT);
  shape_params.texIndex = floorf((float)iZ);
  // for (int i = -50; i < 50; i++) {
  //   for (int j = -50; j < 50; j++) {
  //     float x = ((float)i + j * 0.5f - j / 2) * (2.0f * 0.866025404f);
  //     float z = (float)j * 1.5f;

  //     shape_params.model = HMM_Translate(HMM_Vec3(x - 2, 0.0f, -z));

  //   }
  // }

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

  // if (state.show_mem_ui) {
  sdtx_draw();
  // }
  if (state.show_debug_ui) {
    __cdbgui_draw();
  }
  sg_end_pass();
  sg_commit();
  state.renderTime = stm_diff(stm_now(), renderStartTime);
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
  // char app_title[21];
  // sprintf(app_title, "App Version %s.%s.%s", PROJECT_VERSION_MAJOR,
  //         PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH);
  // app_title[20] = '\0';
  return (sapp_desc){
      .init_cb = init,
      .frame_cb = frame,
      .cleanup_cb = cleanup,
      .event_cb = event,
      .width = SCREEN_WIDTH,
      .height = SCREEN_HEIGHT,
      .gl_force_gles2 = false,
      .window_title = "app_title",
      .sample_count = 4,
      .icon.sokol_default = false,
  };
}
