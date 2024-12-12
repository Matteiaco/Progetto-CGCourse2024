#version 460 core 
layout (location = 0) in vec3 aPosition; 
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec2 aTexCoord;
 
out vec2 vTexCoord;
out vec3 vLdirTS;
out vec3 vFragPos;

uniform mat4 uProj;
uniform mat4 uView;
uniform mat4 uModel;
uniform vec4 uLdir;

void main(void){ 
    // computing the (inverse of the ) tangent frame
    vec3 tangent = normalize(aTangent);
    vec3 bitangent = normalize(cross(aNormal,tangent));
	
	mat3 TF;
	TF[0] = tangent;
	TF[1] = bitangent;
	TF[2] = normalize(aNormal);
	TF = transpose(TF);

	// light direction in tangent space
	vLdirTS = TF * (inverse(uModel) * uLdir).xyz;

	// Calcola la posizione del frammento
	vFragPos = vec3(uModel * vec4(aPosition, 1.0));

	vTexCoord = aTexCoord;
    gl_Position = uProj * uView * uModel * vec4(aPosition, 1.0);
}
