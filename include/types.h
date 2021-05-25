#ifndef _TYPES_H
#define _TYPES_H

#include "sokol_gfx.h"
#include "sokol_fetch.h"

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
#endif _TYPES_H