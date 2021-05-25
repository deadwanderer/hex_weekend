#ifndef _FETCH_H
#define _FETCH_H

#include "types.h"
#include "sokol_app.h"
#include "stb/stb_image.h"

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

#endif