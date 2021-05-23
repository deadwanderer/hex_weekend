#ifndef CAMERA_H
#define CAMERA_H

#include "HandmadeMath/HandmadeMath.h"
#include "sokol_app.h"

enum camera_movement {
  CAMERA_FORWARD,
  CAMERA_BACKWARD,
  CAMERA_LEFT,
  CAMERA_RIGHT,
  CAMERA_UP,
  CAMERA_DOWN
};

typedef struct _Camera_t {
  hmm_vec3 position;
  hmm_vec3 world_up;
  float fov;
  float yaw, pitch, zoom;
  float min_pitch, max_pitch;
  float min_zoom, max_zoom;
  float movement_speed, aim_speed, zoom_speed;
  bool enable_aim;
  bool move_forward, move_backward, move_left, move_right, move_up, move_down;
  hmm_vec3 _front, _up, _right;
} camera_t;

hmm_mat4 camera_get_view_matrix(camera_t* cam) {
  hmm_vec3 target = HMM_AddVec3(cam->position, cam->_front);
  return HMM_LookAt(cam->position, target, cam->_up);
}

hmm_vec3 camera_get_position(camera_t* cam) {
  return cam->position;
}

hmm_vec3 camera_get_direction(camera_t* cam) {
  return cam->_front;
}

float camera_get_fov(camera_t* cam) {
  return cam->fov;
}

void camera_update_vectors(camera_t* cam) {
  hmm_vec3 front;
  front.X = cosf(HMM_ToRadians(cam->yaw)) * cosf(HMM_ToRadians(cam->pitch));
  front.Y = sinf(HMM_ToRadians(cam->pitch));
  front.Z = sinf(HMM_ToRadians(cam->yaw)) * cosf(HMM_ToRadians(cam->pitch));
  cam->_front = HMM_NormalizeVec3(front);
  cam->_right = HMM_NormalizeVec3(HMM_Cross(cam->_front, cam->world_up));
  cam->_up = HMM_NormalizeVec3(HMM_Cross(cam->_right, cam->_front));
}

static void camera_aim(camera_t* cam, hmm_vec2 mouse_offset) {
  cam->yaw += mouse_offset.X * cam->aim_speed;
  cam->pitch += mouse_offset.Y * cam->aim_speed;

  cam->pitch = HMM_Clamp(cam->min_pitch, cam->pitch, cam->max_pitch);

  camera_update_vectors(cam);
}

static void camera_zoom(camera_t* cam, float yoffset) {
  cam->zoom -= yoffset * cam->zoom_speed;
  cam->zoom = HMM_Clamp(cam->min_zoom, cam->zoom, cam->max_zoom);
}

void camera_update(camera_t* cam, float delta_time) {
  float velocity = cam->movement_speed * delta_time;
  if (cam->move_forward) {
    hmm_vec3 offset = HMM_MultiplyVec3f(cam->_front, velocity);
    cam->position = HMM_AddVec3(cam->position, offset);
  }
  if (cam->move_backward) {
    hmm_vec3 offset = HMM_MultiplyVec3f(cam->_front, velocity);
    cam->position = HMM_SubtractVec3(cam->position, offset);
  }
  if (cam->move_left) {
    hmm_vec3 offset = HMM_MultiplyVec3f(cam->_right, velocity);
    cam->position = HMM_SubtractVec3(cam->position, offset);
  }
  if (cam->move_right) {
    hmm_vec3 offset = HMM_MultiplyVec3f(cam->_right, velocity);
    cam->position = HMM_AddVec3(cam->position, offset);
  }
  if (cam->move_up) {
    hmm_vec3 offset = HMM_MultiplyVec3f(cam->_up, velocity);
    cam->position = HMM_AddVec3(cam->position, offset);
  }
  if (cam->move_down) {
    hmm_vec3 offset = HMM_MultiplyVec3f(cam->_up, velocity);
    cam->position = HMM_SubtractVec3(cam->position, offset);
  }
}

void camera_handle_input(camera_t* cam,
                         const sapp_event* e,
                         hmm_vec2 mouse_offset) {
  if (e->type == SAPP_EVENTTYPE_KEY_DOWN) {
    if (e->key_code == SAPP_KEYCODE_W) {
      cam->move_forward = true;
    }
    if (e->key_code == SAPP_KEYCODE_S) {
      cam->move_backward = true;
    }
    if (e->key_code == SAPP_KEYCODE_A) {
      cam->move_left = true;
    }
    if (e->key_code == SAPP_KEYCODE_D) {
      cam->move_right = true;
    }
    if (e->key_code == SAPP_KEYCODE_Q) {
      cam->move_up = true;
    }
    if (e->key_code == SAPP_KEYCODE_Z) {
      cam->move_down = true;
    }
  } else if (e->type == SAPP_EVENTTYPE_KEY_UP) {
    if (e->key_code == SAPP_KEYCODE_W) {
      cam->move_forward = false;
    }
    if (e->key_code == SAPP_KEYCODE_S) {
      cam->move_backward = false;
    }
    if (e->key_code == SAPP_KEYCODE_A) {
      cam->move_left = false;
    }
    if (e->key_code == SAPP_KEYCODE_D) {
      cam->move_right = false;
    }
    if (e->key_code == SAPP_KEYCODE_Q) {
      cam->move_up = false;
    }
    if (e->key_code == SAPP_KEYCODE_Z) {
      cam->move_down = false;
    }
  } else if (e->type == SAPP_EVENTTYPE_MOUSE_DOWN) {
    if (e->mouse_button == SAPP_MOUSEBUTTON_RIGHT) {
      cam->enable_aim = true;
    }
  } else if (e->type == SAPP_EVENTTYPE_MOUSE_UP) {
    if (e->mouse_button == SAPP_MOUSEBUTTON_RIGHT) {
      cam->enable_aim = false;
    }
  } else if (e->type == SAPP_EVENTTYPE_MOUSE_SCROLL) {
    camera_zoom(cam, e->scroll_y);
  } else if (e->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
    if (cam->enable_aim) {
      camera_aim(cam, mouse_offset);
    }
  }
}

void camera_set_up(camera_t* cam, hmm_vec3 position) {
  cam->position = position;
  cam->world_up = HMM_Vec3(0.0f, 1.0f, 0.0f);
  cam->yaw = -90.0f;
  cam->pitch = 0.0f;
  cam->zoom = 45.0f;
  cam->fov = 45.0f;
  cam->movement_speed = 5.f;
  cam->aim_speed = 1.0f;
  cam->zoom_speed = 0.1f;
  cam->min_pitch = -89.0f;
  cam->max_pitch = 89.0f;
  cam->min_zoom = 1.0f;
  cam->max_zoom = 45.0f;
  cam->move_forward = false;
  cam->move_backward = false;
  cam->move_left = false;
  cam->move_right = false;
  cam->enable_aim = false;

  camera_update_vectors(cam);
}

#endif