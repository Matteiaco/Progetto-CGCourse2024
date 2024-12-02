#version 460 core  
out vec4 color; 

in vec2 vTexCoord;
in vec3 vLdirTS;
in vec3 vFragPos;

uniform int uRenderMode;
uniform vec3 lightColor;
uniform int dayTime;

uniform sampler2D TextureSampler;
uniform sampler2D NormalMapSampler;

// Definizione per SpotLight
struct SpotLight {
    vec3 position;      // Posizione della spotlight
    vec3 direction;     // Direzione della spotlight
    vec3 color;         // Colore della luce
    float cutOff;       // Angolo interno del cono della spotlight (coseno dell'angolo)
    float outerCutOff;  // Angolo esterno del cono (coseno dell'angolo)
    float constant;     // Attenuazione costante
    float linear;       // Attenuazione lineare
    float quadratic;    // Attenuazione quadratica
};

#define NR_SPOTLIGHTS 19
uniform SpotLight spotLights[NR_SPOTLIGHTS];

// Diffuse per luce direzionale
vec3 diffuse( vec3 L, vec3 N){
	return max(0.0, dot(L, N)) * texture(TextureSampler, vTexCoord).rgb;
}

// Diffuse per spotlight
vec3 spotLightDiffuse(SpotLight light, vec3 N) {
    // Direzione della luce verso il frammento
    vec3 lightDir = normalize(light.position - vFragPos);
    
    // Controlla se il frammento è all'interno del cono della spotlight
    float theta = dot(lightDir, normalize(-light.direction));
    
    // Calcola l'attenuazione del cono
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    
    // Diffuse lighting
    float diff = max(dot(N, lightDir), 0.0);
    
    // Attenuazione in base alla distanza
    float distance = length(light.position - vFragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    // Combina i contributi
    vec3 result = light.color * diff * attenuation * intensity;
    return result;
}


void main(void)
{ 
    vec3 N = normalize(texture(NormalMapSampler, vTexCoord).rgb * 2.0 - 1.0);  // Normale dallo spazio texture
    
    // Luce direzionale
    vec3 lighting = vec3(0.0);
	if(uRenderMode==0)// only color
	{
		color = texture2D(TextureSampler,vTexCoord);
	}else
    if (uRenderMode == 1)  // Calcolo della luce solo se siamo in modalità "normal mapping"
    {   
        if(dayTime == 1){
            // Luce diffusa direzionale
            lighting += diffuse(normalize(vLdirTS), N) * lightColor;  
        }else {   
            // Aggiunta delle spotlights
            for (int i = 0; i < NR_SPOTLIGHTS; i++) {
                lighting += spotLightDiffuse(spotLights[i], N);  // Aggiunge l'illuminazione delle spotlights
            }
            lighting = lighting * diffuse(normalize(vLdirTS), N); //Aggiunge il colore degli oggetti
        }
    	color = vec4(lighting, 1.0);  // Colore finale
    } 
} 
