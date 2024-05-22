#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout(location = 0) out vec4 fragColor;

layout(set = 1, binding = 0) uniform sampler2D AudioAnalysis;

#define PI 3.1415926
// http://github.prideout.net/barrel-distortion/
vec2 distort(vec2 p, float power)
{
    float a  = atan(p.y, p.x);
    float r = length(p);
    r = pow(r, power);
   // return vec2(r * cos(a), r*sin(a));
    return vec2((a / PI), r*2.0-1.0);	// polar
}

// 2D LED Spectrum - Visualiser
// Based on Led Spectrum Analyser by: simesgreen - 27th February, 
// 2013 https://www.shadertoy.com/view/Msl3zr
// 2D LED Spectrum by: uNiversal - 27th May, 2015
// Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
void main()
{
    // create pixel coordinates
    vec2 uv = gl_FragCoord.xy / ubo.iResolution.xy;
    uv.y = 1.0f - uv.y;
   
    // distort
    float bass = texture( AudioAnalysis, vec2(0, 0) ).x * 12;
    //uv = distort(uv*2.0-1.0, 0.5+bass)*0.5+0.5;

    // quantize coordinates
    const float bands = 100;
    const float segs = 100;
    vec2 p;
    p.x = floor(uv.x*bands)/bands;
    p.y = floor(uv.y*segs)/segs;

    // read frequency data from first row of texture
    float fft1  = texture( AudioAnalysis, vec2(p.x, 0.26) ).x;
    float fft2  = texture( AudioAnalysis, vec2(p.x, 0.0) ).x;

    // led color
    vec3 color1 = mix(vec3(0.0, 2.0, 0.0), vec3(2.0, 0.0, 0.0), sqrt(uv.y * 2));
    vec3 color2 = mix(vec3(0.0, 2.0, 0.0), vec3(2.0, 0.0, 0.0), sqrt((uv.y - .5) * 2.0));
    color1 = clamp(color1, 0, 1);
    color2 = clamp(color2, 0, 1);

    fft1 = min(fft1, 1.0);
    fft2 = min(fft2, 1.0);
    // mask for bar graph
    float mask1 = (p.y < (fft1 * .5)) ? 1.0 : 0.05;
    float mask2 = (p.y - .5 < fft2 * .5) ? 1.0 : 0.05;

    // led shape
    vec2 d = fract((uv - p) *vec2(bands, segs)) - 0.5;
    float led = smoothstep(0.5, 0.35, abs(d.x)) *
                smoothstep(0.5, 0.35, abs(d.y));
    vec3 ledColor = led*color1*mask1;
    ledColor += led*color2*mask2;

    // second texture row is the sound wave
    float wave = texture( AudioAnalysis, vec2(uv.x, 0.75) ).x;
    vec3 waveColor = vec3(0.001, 0.01, 0.04) / abs(wave - uv.y + 0.66);

    float wave2 = texture( AudioAnalysis, vec2(uv.x, 0.55) ).x;
    vec3 waveColor2 = vec3(0.04, 0.01, 0.001) / abs(wave - uv.y + 0.366);

    ledColor += waveColor + waveColor2;

    // output final color
    fragColor = vec4(ledColor, 1.0);
    //fragColor = vec4(texture(AudioAnalysis, vec2(uv.x, 0.0f)).x, 0.0f, 0.0f, 1.0f) * 8.0;
}
