#version 450
#include "atmosphere_common.glsl"

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 4) uniform sampler2D skyViewLUT;
layout(set = 0, binding = 2) uniform sampler2D transmittanceLUT;

layout(std140, set = 0, binding = 1) uniform AtmosphereUBO {
    AtmosphereParameters params;
    vec4 CameraPosition;
    mat4 InvViewProj;
    vec4 PostProcessParams; // x = exposure, y = gamma, zw = viewportSize
    vec4 AtmosphereFlags;   // x = EnableMultiScattering, y = EnableAerialPerspective, z = SkyIntensity, w = padding
} ubo;

void main() {
    // Reconstruct view ray in world space
    // Reconstruct NDC position from UV (Y is down in Vulkan, so we invert Y for NDC)
    vec2 ndcUV = inUV * 2.0 - 1.0;
    vec4 clipPos = vec4(ndcUV.x, ndcUV.y, 1.0, 1.0);
    vec4 worldPos = ubo.InvViewProj * clipPos;
    worldPos /= worldPos.w;
    
    // ubo.CameraPosition is now in km and offset by PlanetRadius!
    // Reconstruct camera pos in meters for view direction calculation
    vec3 camPosMeters = ubo.CameraPosition.xyz * 1000.0 - vec3(0.0, ubo.params.PlanetRadius * 1000.0, 0.0);
    vec3 d = normalize(worldPos.xyz - camPosMeters);

    // Camera origin relative to planet center (already in km)
    vec3 camPosWorld = ubo.CameraPosition.xyz;
    float r = length(camPosWorld);

    // Set up local coordinate frame where camera is at (0, r, 0)
    // The zenith axis is Z_local
    vec3 Z = camPosWorld / r;
    vec3 sunDirWorld = ubo.params.SunDirectionAndIntensity.xyz;
    
    // Projection of sun onto horizon plane
    vec3 X;
    float dotSZ = dot(sunDirWorld, Z);
    if (abs(dotSZ) > 0.999) {
        X = normalize(vec3(0.0, 1.0, 0.0) - dot(vec3(0.0, 1.0, 0.0), Z) * Z);
    } else {
        X = normalize(sunDirWorld - dotSZ * Z);
    }
    vec3 Y = cross(Z, X);

    vec3 dLocal = vec3(dot(d, X), dot(d, Z), dot(d, Y));
    
    // Spherical coordinates
    float theta = acos(clamp(dLocal.y, -1.0, 1.0));
    float phi = atan(dLocal.z, dLocal.x);
    if (phi < 0.0) phi += 2.0 * PI;

    // Sample sky color from LUT and apply sky intensity
    vec2 skyViewUV = SkyViewParamsToUV(ubo.params, theta, phi, r);
    vec3 color = texture(skyViewLUT, skyViewUV).rgb * ubo.AtmosphereFlags.z;

    // Render physical sun disk
    float sunCosine = dot(d, sunDirWorld);
    float sunAngularRadiusCos = 0.99995; // about 0.545 degrees
    
    if (sunCosine > sunAngularRadiusCos) {
        // Find transmittance to sun
        vec3 sunTransmittance = getTransmittance(ubo.params, transmittanceLUT, r, dot(Z, sunDirWorld));
        
        // Sun intensity and color
        vec3 sunColor = ubo.params.SunColor.rgb * ubo.params.SunDirectionAndIntensity.w;
        
        // Soft edge
        float softSun = smoothstep(sunAngularRadiusCos, sunAngularRadiusCos + 0.00002, sunCosine);
        
        // Add to sky color
        color += sunColor * sunTransmittance * softSun;
    }

    // Apply exposure and tone mapping
    float exposure = ubo.PostProcessParams.x;
    float gamma = ubo.PostProcessParams.y;
    
    vec3 tonemapped = vec3(1.0) - exp(-color * exposure);
    vec3 corrected = pow(tonemapped, vec3(1.0 / gamma));

    outColor = vec4(corrected, 1.0); 
}
