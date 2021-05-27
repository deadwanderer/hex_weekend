#ifndef _FETCH_H
#define _FETCH_H

#include "types.h"
#include "sokol_app.h"
#include "stb/stb_image.h"
#include <string.h>

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

static bool _load_arraytex(_arraytex_request_t* request) {
  const int desired_channels = 4;
  int img_widths[ARRAYTEX_COUNT], img_heights[ARRAYTEX_COUNT];
  sg_image_data img_data;

  for (int i = 0; i < ARRAYTEX_COUNT; i++) {
    int num_channel;
    stbi_uc* img_ptr =
        stbi_load_from_memory(request->buffer + (i * request->buffer_offset),
                              request->fetched_sizes[i], &img_widths[i],
                              &img_heights[i], &num_channel, desired_channels);
    memcpy(request->texture_buffer_ptr +
               (i * ARRAYTEX_ARRAY_IMAGE_OFFSET * sizeof(uint32_t)),
           img_ptr,
           img_widths[i] * img_heights[i] * desired_channels * sizeof(stbi_uc));
    stbi_image_free(img_ptr);
  }
  img_data.subimage[0][0] =
      (sg_range){.ptr = request->texture_buffer_ptr,
                 .size = ARRAYTEX_IMAGE_BUFFER_SIZE * sizeof(uint32_t)};

  /*
    static uint32_t pixels[3][16][16];
    for (int layer = 0, even_odd = 0; layer < 16; layer++) {
      for (int y = 0; y < 16; y++, even_odd++) {
        for (int x = 0; x < 16; x++, even_odd++) {
          if (even_odd & 1) {
            switch (layer) {
              case 0:
                pixels[layer][y][x] = 0x000000FF;
                break;
              case 1:
                pixels[layer][y][x] = 0x0000FF00;
                break;
              case 2:
                pixels[layer][y][x] = 0x00FF0000;
                break;
            }
          } else {
            pixels[layer][y][x] = 0;
          }
        }
      }
    }

    img_data.subimage[0][0] = SG_RANGE(pixels);
  */

  bool valid = img_heights[0] > 0 && img_widths[0] > 0;

  for (int i = 1; i < ARRAYTEX_COUNT; i++) {
    if (img_widths[i] != img_widths[0] || img_heights[i] != img_heights[0] ||
        img_widths[i] != ARRAYTEX_IMAGE_WIDTH ||
        img_heights[i] != ARRAYTEX_IMAGE_HEIGHT) {
      valid = false;
      break;
    }
  }

  if (valid) {
    sg_init_image(request->img_id,
                  &(sg_image_desc){.type = SG_IMAGETYPE_ARRAY,
                                   .width = img_widths[0],
                                   .height = img_heights[0],
                                   .num_slices = ARRAYTEX_COUNT,
                                   .pixel_format = SG_PIXELFORMAT_RGBA8,
                                   .min_filter = SG_FILTER_LINEAR,
                                   .mag_filter = SG_FILTER_LINEAR,
                                   .data = img_data,
                                   .label = "arraytex-image"});
  }

  return valid;
}

static void arraytex_fetch_callback(const sfetch_response_t* response) {
  _arraytex_request_instance_t req_inst =
      *(_arraytex_request_instance_t*)response->user_data;
  _arraytex_request_t* request = req_inst.request;

  if (response->fetched) {
    request->fetched_sizes[req_inst.index] = response->fetched_size;
    ++request->finished_requests;
  } else if (response->failed) {
    request->failed = true;
    ++request->finished_requests;
  }

  if (request->finished_requests == ARRAYTEX_COUNT) {
    if (!request->failed) {
      request->failed = !_load_arraytex(request);
    }

    if (request->failed) {
      request->fail_callback();
    } else {
      request->success_callback();
    }
  }
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
    } else {
      request->success_callback();
    }
  }
}

#endif