#version 330 core
in vec3 TexCoord;
in vec4 VertexColor;

uniform sampler2DArray textureAtlas;

void main() {
    vec4 texColor = texture(textureAtlas, TexCoord);
    
    // Transparent elements like glass/leaves edge gaps don't cast shadows
    // And completely ignore Water (VertexColor.a 0.65-0.75) so light pierces the ocean!
    if (texColor.a < 0.1 || (VertexColor.a > 0.65 && VertexColor.a < 0.75)) {
        discard;
    }
}
