#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main() {
    // Very simple directional light logic for testing
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(fragNormal, lightDir), 0.1); // 0.1 ambient
    
    vec3 resultColor = fragColor * diff;
    outColor = vec4(resultColor, 1.0);
}
