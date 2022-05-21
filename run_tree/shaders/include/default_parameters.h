struct Channel 
{
    vec4 resolution;
    float time;
 };

// For now this has to match the source
layout(set = 0, binding = 0) uniform UBO
{
    float iTime;            // Elapsed 
    float iGlobalTime;      // Elapsed (same as iTime)
    float iTimeDelta;       // Delta since last frame
    float iFrame;           // Number of frames drawn since begin
    float iFrameRate;       // 1 / Elapsed
    float iSampleRate;      // Sound sample rate
    vec4 iResolution;  // Resolution of current target
    vec4 iMouse;       // Mouse coords in pixels
    vec4 iDate;        // Year, Month, Day, Seconds since epoch
        
    vec4 iChannelTime; // Time for an input channel

    vec4 iChannelResolution[4];    // Resolution for an input channel
    vec4 ifFragCoordOffsetUniform;
    vec4 eye;                      // The eye in world space
    
    mat4 model;                    // Transforms for camera based rendering
    mat4 view;
    mat4 projection;
    mat4 modelViewProjection;
    
    Channel iChannel[4];                // Packed version
    

} ubo; 
