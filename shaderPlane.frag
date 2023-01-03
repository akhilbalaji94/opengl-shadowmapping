#version 330 core

in vec3 view_pos;
in vec3 view_normal;
in vec3 world_normal;
in vec3 world_pos;
in vec3 world_view_pos;
in vec4 lightview_pos;

uniform vec4 camera_pos;
uniform vec4 light_pos;
uniform sampler2DShadow model_shadow;
uniform vec2 reflection_size;

layout(location = 0) out vec3 color;

void main()
{
    vec3 diffuse_color = vec3(0.8, 0.8, 0.8);
    vec3 light_color = vec3(1.0, 1.0, 1.0);
    vec3 ambient_light_color = vec3(0.2, 0.2, 0.2);
    vec3 specular_color = vec3(0.8, 0.8, 0.8);
    float specular_power = 20.0;
    vec3 view_dir = normalize(camera_pos.xyz - world_pos);
    vec3 half_vector = normalize(light_pos.xyz - view_dir.xyz);
    vec3 env_reflect = reflect(-(view_dir), world_normal);
    vec3 lightview_p = (lightview_pos.xyz / lightview_pos.w);
    if (lightview_p.x > 0.0 && lightview_p.x < 1 && lightview_p.y > 0.0 && lightview_p.y < 1)
    {
        light_color *= textureProj(model_shadow, lightview_pos) < lightview_pos.z/lightview_pos.w ? 0.0 : 1.0;
    }
    vec3 diffuse_shading = (light_color * clamp(dot(normalize(view_normal), normalize(light_pos.xyz - view_pos)), 0, 1) * diffuse_color);
    vec3 specular_shading = (light_color * pow(clamp(dot(normalize(view_normal), normalize(half_vector)), 0, 1), specular_power) * specular_color);
    vec3 ambient_shading = (ambient_light_color * diffuse_color);

    color = (diffuse_shading + specular_shading + ambient_shading);
}