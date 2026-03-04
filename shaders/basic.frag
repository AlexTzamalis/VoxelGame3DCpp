#version 330 core
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

uniform sampler2D textureAtlas;
uniform vec2 atlasTileSize;
uniform vec2 atlasOffset;
uniform vec3 lightDir;
uniform vec3 lightColor;

void main() {
    vec4 texColor = texture(textureAtlas, TexCoord * atlasTileSize + atlasOffset);
    float diff = max(dot(normalize(Normal), normalize(-lightDir)), 0.0);
    vec3 diffuse = diff * lightColor;
    vec3 ambient = 0.3 * lightColor;
    FragColor = vec4(ambient + diffuse, 1.0) * texColor;
}
