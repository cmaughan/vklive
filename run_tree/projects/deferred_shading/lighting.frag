#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (set = 1, binding = 0) uniform sampler2D Positions;
layout (set = 1, binding = 1) uniform sampler2D Albedo;
layout (set = 1, binding = 2) uniform sampler2D Normals;

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec3 inEye;

layout(location = 0) out vec4 outFragcolor;

int displayDebugTarget = 0;

struct Light {
    vec4 position;
    vec3 color;
    float radius;
};

#define num_lights 3
Light lights[num_lights] = Light[num_lights](
Light( vec4(0.0f, -2.0f, -2.0f, 1.0f), vec3(1.0f, 0.0f, 0.0f), 2.0f ),
Light( vec4(-1.0f, -2.0f, -2.0f, 1.0f), vec3(0.0f, 1.0f, 0.0f), 2.0f ),
Light( vec4(0.5f, -1.0f, -0.5f, 1.0f), vec3(0.0f, 0.2f, 1.0f), 2.0f )
);

void main()
{
    // Get G-Buffer values
    vec3 fragPos = texture(Positions, inUV).rgb;
    vec3 normal = texture(Normals, inUV).rgb;
    vec4 albedo = texture(Albedo, inUV);
    
    // Debug display
    if (displayDebugTarget > 0) {
        switch (displayDebugTarget) 
        {
            case 1: 
                outFragcolor.rgb = fragPos;
                break;
            case 2: 
                outFragcolor.rgb = normal;
                break;
            case 3: 
                outFragcolor.rgb = albedo.rgb;
                break;
            case 4: 
                outFragcolor.rgb = albedo.aaa;
                break;
        }		
        outFragcolor.a = 1.0;
        return;
    }

    // Render-target composition

    #define ambient 0.13
    
    // Ambient part
    vec3 fragcolor  = albedo.rgb * ambient;

    for(int i = 0; i < num_lights; ++i)
    {
        // Vector to light
        vec3 L = lights[i].position.xyz - fragPos;

        // Distance from light to fragment position
        float dist = length(L);

        // Viewer to fragment
        vec3 V = inEye - fragPos;
        V = normalize(V);
        
        //if(dist < ubo.lights[i].radius)
        {
            // Light to fragment
            L = normalize(L);

            // Attenuation
            float atten = lights[i].radius / (pow(dist, 2.0) + 1.0);

            // Diffuse part
            vec3 N = normalize(normal);
            float NdotL = max(0.0, dot(N, L));
            vec3 diff = lights[i].color * albedo.rgb * NdotL * atten;

            // Specular part
            // Specular map values are stored in alpha of albedo mrt
            vec3 R = reflect(-L, N);
            float NdotR = max(0.0, dot(R, V));
            vec3 spec = lights[i].color * albedo.a * pow(NdotR, 16.0) * atten;

            fragcolor += diff + spec;	
        }	
    }    	
   
  outFragcolor = vec4(fragcolor, 1.0);
}