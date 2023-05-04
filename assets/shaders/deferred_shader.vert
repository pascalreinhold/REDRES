#version 450

layout (location = 0) out vec2 uv_out;

void main()
{
    // (i << 1) & 2 is equivalent to 2*i & 2 or but faster?
    uv_out = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv_out * 2.0f - 1.0f, 0.0f, 1.0f);
}