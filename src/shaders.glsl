@ctype vec2 hmm_vec2
@ctype vec3 hmm_vec3
@ctype vec4 hmm_vec4
@ctype mat4 hmm_mat4

@vs vs
  layout(location = 0) in vec3 pos;
  void main() {
    gl_Position = vec4(pos, 1.0);
  }
@end

@fs fs
out vec4 FragColor;

void main() {
  FragColor = vec4(1.0, 0.0, 1.0, 1.0);
}
@end

@program Shader vs fs