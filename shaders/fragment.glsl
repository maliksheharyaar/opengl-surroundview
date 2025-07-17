#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

uniform sampler2D imageTexture;
uniform sampler2D texture_diffuse1;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform int useTexture;
uniform vec3 solidColor;
uniform vec3 materialDiffuse;
uniform vec3 materialSpecular;
uniform vec3 materialAmbient;

void main() {
    vec3 color;
    
    if (useTexture == 1) {
        // Use image texture (for the plane) - no lighting adjustment needed
        color = texture(imageTexture, TexCoord).rgb;
        FragColor = vec4(color, 1.0);
    } else if (useTexture == 2) {
        // Use GLB model diffuse texture
        color = texture(texture_diffuse1, TexCoord).rgb;
        
        // Add proper directional lighting for 3D model
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * color;
        
        vec3 ambient = 0.7 * color;
        vec3 result = ambient + diffuse;
        FragColor = vec4(result, 1.0);
    } else {
        // Use material colors from GLB file
        color = materialDiffuse;
        
        // Add proper directional lighting for 3D model
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * color;
        
        vec3 ambient = 0.7 * color;
        vec3 result = ambient + diffuse;
        FragColor = vec4(result, 1.0);
    }
}
