@ctype vec2 hmm_vec2
@ctype vec3 hmm_vec3
@ctype vec4 hmm_vec4
@ctype mat4 hmm_mat4

@vs vs
uniform vs_params {
    mat4 mvp;
};

in vec4 position;
in vec4 color0;

out vec4 color;

void main() {
    gl_Position = mvp * position;
    color = color0;
}
@end

@fs fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

@vs textured_vs
uniform vs_params {
    mat4 mvp;
};

in vec4 position;
in vec2 texcoord0;

out vec2 texcoord;

void main() {
    gl_Position = mvp * position;
    texcoord = texcoord0;
}
@end

@fs textured_fs
in vec2 texcoord;
out vec4 frag_color;

uniform sampler2D cube_texture;

void main() {
    frag_color = texture(cube_texture, texcoord);
}
@end

@vs shape_vs
uniform shape_vs_params {
    mat4 mvp;
};

layout(location=0) in vec4 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;
layout(location=3) in vec4 color0;

out vec4 color;
//out vec3 out_normal;
//out vec2 out_texcoord;

void main() {
    gl_Position = mvp * position;
    color = color0;    
    //out_normal = normal;
    //out_texcoord = texcoord;
}
@end

@vs textured_shape_vs
uniform textured_shape_vs_params {
    mat4 model;
    mat4 viewproj;
};

layout(location=0) in vec4 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;
layout(location=3) in vec4 color0;
layout(location=4) in vec3 inst_pos;
layout(location=5) in float texIndex;

out vec4 color;
//out vec3 out_normal;
out vec2 out_texcoord;
out vec3 array_texcoord;
out vec3 world_position;

void main() {
    vec4 worldpos = (position+vec4(inst_pos, 1.0));
    gl_Position = viewproj * model * worldpos;
    color = color0;    
    //out_normal = normal;
    out_texcoord = texcoord;
    array_texcoord = vec3(texcoord, texIndex);
    world_position = (model * position).xyz;
}
@end

@fs shape_fs
in vec4 color;
//in vec3 out_normal;
//in vec2 out_texcoord;
out vec4 frag_color;

void main() {
    //vec3 norm = normalize(out_normal);
    //vec2 redo = out_texcoord * vec2(0.4, 0.4);
    //frag_color = color * vec4(norm, 0.5) * vec4(redo, 1.0, 0.5);
    frag_color = color;
}
@end

@fs textured_shape_fs
in vec4 color;
//in vec3 out_normal;
in vec2 out_texcoord;
in vec3 array_texcoord;
in vec3 world_position;
out vec4 frag_color;

uniform sampler2D shape_texture;
uniform sampler2DArray shape_arraytex;

void main() {
    //vec3 norm = normalize(out_normal);
    //vec2 redo = out_texcoord * vec2(0.4, 0.4);
    //frag_color = color * vec4(norm, 0.5) * vec4(redo, 1.0, 0.5);
    //frag_color = color;
    frag_color = texture(shape_texture, (world_position * 0.05).xz);
    frag_color = texture(shape_arraytex, array_texcoord);
}
@end

@vs vs_skybox
in vec3 a_pos;

out vec3 tex_coords;

uniform skybox_vs_params {
    mat4 view;
    mat4 projection;
};

void main() {
    tex_coords = a_pos;
    vec4 pos = projection * view * vec4(a_pos, 1.0);
    gl_Position = pos.xyww;
}
@end

@fs fs_skybox
in vec3 tex_coords;

out vec4 frag_color;

uniform samplerCube skybox_texture;

void main() {
    frag_color = texture(skybox_texture, tex_coords);
}
@end

@program skybox vs_skybox fs_skybox
@program cube vs fs
@program textured_cube textured_vs textured_fs
@program shape shape_vs shape_fs
@program textured_shape textured_shape_vs textured_shape_fs