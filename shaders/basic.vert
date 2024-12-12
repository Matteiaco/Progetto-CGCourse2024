#version 460 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec2 TexCoord;
out vec3 FragPos; // Posizione del frammento per il calcolo della luce
out vec3 Normal;
out mat3 TBN;

void main(){
    FragPos = vec3(uModel * vec4(inPosition, 1.0));
    gl_Position = uProj * uView * vec4(FragPos, 1.0);

    Normal = normalize(mat3(transpose(inverse(uModel))) * inNormal); // Trasformazione delle normali
    
    TexCoord = inTexCoord;

    vec3 T = normalize(mat3(uModel) * inTangent);
    vec3 B = cross(Normal, T);
    TBN = mat3(T, B, Normal);
}
