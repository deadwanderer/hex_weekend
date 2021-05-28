@ctype vec2 hmm_vec2
@ctype vec3 hmm_vec3
@ctype vec4 hmm_vec4
@ctype mat4 hmm_mat4

@block util
vec4 encodeDepth(float v) {
    vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0) * v;
    enc = fract(enc);
    enc -= enc.yzww * vec4(1.0/255.0,1.0/255.0, 1.0/255.0, 0.0);
    return enc;
}

float decodeDepth(vec4 rgba) {
    return dot(rgba, vec4(1.0, 1.0/255.0, 1.0/65025.0, 1.0/16581375.0));
}

float sampleShadow(sampler2D shadowMap, vec2 uv, float compare) {
    #if !SOKOL_GLSL
    uv.y = 1.0 - uv.y;
    #endif
    float depth = decodeDepth(texture(shadowMap, vec2(uv.x, uv.y)));
    depth += 0.001;
    return step(compare, depth);
}

float sampleShadowPCF(sampler2D shadowMap, vec2 uv, vec2 smSize, float compare) {
    float result = 0.0;
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            vec2 off = vec2(x,y)/smSize;
            result += sampleShadow(shadowMap, uv+off, compare);
        }
    }
    return result / 25.0;
}

vec4 gamma(vec4 c) {
    float p = 1.0 / 2.2;
    return vec4(pow(c.xyz, vec3(p,p,p)), c.w);
}
@end

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

uniform sampler2D shadowMap;
uniform sampler2DArray shape_arraytex;

void main() {
    //vec3 norm = normalize(out_normal);
    //vec2 redo = out_texcoord * vec2(0.4, 0.4);
    //frag_color = color * vec4(norm, 0.5) * vec4(redo, 1.0, 0.5);
    //frag_color = color;
    frag_color = texture(shadowMap, (world_position * 0.05).xz);
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

@vs shadowVS
uniform vs_shadow_params {
    mat4 mvp;
};

in vec4 position;
in vec3 inst_pos;
out vec2 projZW;
void main() {
    vec4 instPos = position + vec4(inst_pos, 1.0);
    gl_Position = mvp * instPos;
    projZW = gl_Position.zw;
}
@end

@fs shadowFS
@include_block util
in vec2 projZW;
out vec4 fragColor;

void main() {
    float depth = projZW.x / projZW.y;
    fragColor = encodeDepth(depth);
}
@end

@program skybox vs_skybox fs_skybox
@program cube vs fs
@program textured_cube textured_vs textured_fs
@program shape shape_vs shape_fs
@program textured_shape textured_shape_vs textured_shape_fs
@program shadow shadowVS shadowFS