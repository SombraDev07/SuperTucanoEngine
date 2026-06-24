#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragPosWorld;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 albedoFactor;
    float metallicFactor;
    float roughnessFactor;
} push;

layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessMap;

const float PI = 3.14159265359;

// GGX/Towbridge-Reitz normal distribution
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / max(denom, 0.0000001);
}

// Schlick-GGX geometry shadowing
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Fresnel Schlick
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // Simple directional light hardcoded for now
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 lightColor = vec3(1.0, 1.0, 1.0) * 3.0; // 3 intensity

    vec3 camPos = vec3(0.0, 0.0, -3.0); // Simple cam pos
    // Read textures
    vec4 albedoTex = texture(albedoMap, fragTexCoord);
    vec3 normalTex = texture(normalMap, fragTexCoord).rgb;
    vec3 mrTex = texture(metallicRoughnessMap, fragTexCoord).rgb;

    // Calculate normal using screen-space derivatives for TBN matrix
    vec3 N = normalize(fragNormal);
    if (!gl_FrontFacing) {
        N = -N;
    }
    vec3 tangentNormal = normalTex * 2.0 - 1.0;
    
    vec3 dp1 = dFdx(fragPosWorld);
    vec3 dp2 = dFdy(fragPosWorld);
    vec2 duv1 = dFdx(fragTexCoord);
    vec2 duv2 = dFdy(fragTexCoord);
    
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    
    float invmax = inversesqrt(max(dot(T,T), dot(B,B)));
    mat3 TBN = mat3(T * invmax, B * invmax, N);
    
    N = normalize(TBN * tangentNormal);
    
    vec3 V = normalize(camPos - fragPosWorld);
    vec3 L = lightDir;
    vec3 H = normalize(V + L);
    
    vec3 albedo = push.albedoFactor.rgb * albedoTex.rgb * fragColor;
    float metallic = push.metallicFactor * mrTex.b;
    float roughness = push.roughnessFactor * mrTex.g;
    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // Calculate per-light radiance
    vec3 radiance = lightColor;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);       
        
    vec3 numerator    = NDF * G * F; 
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular     = numerator / denominator;
        
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;	  
        
    float NdotL = max(dot(N, L), 0.0);        
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

    vec3 ambient = vec3(0.03) * albedo * 1.0; // Hardcoded AO
    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // Gamma correction
    color = pow(color, vec3(1.0/2.2)); 

    outColor = vec4(color, push.albedoFactor.a);
}
