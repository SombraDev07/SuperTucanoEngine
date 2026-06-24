#version 450

layout(location = 0) out vec2 outUV;

void main() {
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    // Position at far plane (depth = 1.0)
    gl_Position = vec4(outUV.x * 2.0 - 1.0, outUV.y * 2.0 - 1.0, 1.0, 1.0);
}
