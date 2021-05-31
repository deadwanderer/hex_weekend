@ctype vec2 hmm_vec2
@ctype vec3 hmm_vec3
@ctype vec4 hmm_vec4
@ctype mat4 hmm_mat4

@vs litTexVS
uniform lit_tex_vs_params {
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
out vec3 out_normal;
out vec2 out_texcoord;
out vec3 array_texcoord;
out vec3 world_position;

void main() {
    vec4 worldpos = (position+vec4(inst_pos, 1.0));    
    gl_Position = viewproj * model * worldpos;
    color = color0;    
    out_normal = normal;
    out_texcoord = texcoord;
    array_texcoord = vec3(texcoord, texIndex);
    world_position = (model * worldpos).xyz;
}
@end

@fs litTexFS
in vec4 color;
in vec3 out_normal;
in vec2 out_texcoord;
in vec3 array_texcoord;
in vec3 world_position;
out vec4 frag_color;

uniform lit_tex_fs_params {
    vec3 lightPos;
    vec3 lightColor;
    vec3 viewPos;
    float ambientStrength;
    float specularStrength;
};

// uniform sampler2D shape_texture;
uniform sampler2DArray shape_arraytex;

void main() {
  vec3 norm = normalize(out_normal);
  vec3 lightDir = normalize(lightPos - world_position);
  vec3 viewDir = normalize(viewPos - world_position);
  vec3 halfwayDir = normalize(lightDir + viewDir);
  // vec3 reflectDir = reflect(-lightDir, norm);
  float diff = max(dot(norm, lightDir), 0.0);
  vec3 diffuse = diff * lightColor;
  float spec = pow(max(dot(norm, halfwayDir), 0.0), 32);
  vec3 specular = specularStrength * spec * lightColor;
  vec4 texColor = texture(shape_arraytex, array_texcoord);
  vec3 ambient = lightColor * ambientStrength;
  frag_color = vec4(texColor * vec4(ambient+diffuse+specular, 1.0));
}
@end

@program lit_tex litTexVS litTexFS