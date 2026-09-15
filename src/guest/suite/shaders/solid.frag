#version 450
// VulkanProbe reads back every pixel expecting exactly this colour.

layout(location = 0) out vec4 colour;

void main() {
    colour = vec4(0.25, 0.5, 0.75, 1.0);
}
