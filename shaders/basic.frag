#version 460 core

in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;

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

uniform vec3 sunlightDirection; // Direzione della luce del sole
uniform vec3 lightColor; // Colore della luce del sole
uniform vec3 ambientColor;  // Colore della luce ambientale
uniform vec3 viewPos; // Posizione della camera
uniform mat4 uModel;
uniform int dayTime;

out vec4 FragColor;

// Funzione per calcolare la luce spot
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir){
    vec3 lightDir = normalize(light.position - fragPos);
    float theta = dot(lightDir, normalize(-light.direction));

    // Verifica se il frammento è dentro il cono di luce
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    // Calcola l'attenuazione
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    // Illuminazione diffusa
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Illuminazione speculare (usando il modello Phong)
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);  // shininess di 32

    // Calcola il colore finale (diffuso + speculare) moltiplicato per l'intensità
    vec3 ambient = 0.1 * light.color;  // valore fisso per l'illuminazione ambientale
    vec3 diffuse = diff * light.color;
    vec3 specular = spec * light.color;

    return (ambient + intensity * (diffuse + specular)) * attenuation;
}

void main()
{
    vec3 result = vec3(0.0);

    // Colore del materiale
    vec3 textureColor = texture(TextureSampler, TexCoord).rgb;

    // Campiona la normal map e calcola la normale del frammento
    vec3 normalFromNormalMap = texture(NormalMapSampler, TexCoord).rgb;
    normalFromNormalMap = normalize(normalFromNormalMap * 2.0 - 1.0); // Converti [0, 1] a [-1, 1]
        
    // Trasforma la normale dal normal map dallo spazio della texture allo spazio del mondo
    vec3 norm = normalize(mat3(uModel) * normalFromNormalMap); 

    // Direzione della luce
    vec3 lightDir = normalize(sunlightDirection); 
        
    // Direzione della vista
    vec3 viewDir = normalize(viewPos - FragPos); 

    // Calcolo della luce ambientale
    vec3 ambient = ambientColor * textureColor;

    // Calcolo della luce diffusa
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * (lightColor * dayTime) * textureColor;

    // Calcolo della luce speculare
    vec3 reflectDir = reflect(-lightDir, norm); 
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128.0); // Specular exponent
    vec3 specular = spec * (lightColor * dayTime) * vec3(0.1, 0.1, 0.1); // Intensità e colore speculare moltiplicato per un fattore di scala per gestire la lucentezza del riflesso

    // Colore finale
    result = ambient + diffuse + specular;

    if (dayTime == 0){
        
        // Calcola il colore derivante dalla luce spot
        vec3 lamps = vec3(0.0);
        for (int i = 0; i < NR_SPOTLIGHTS; i++){
            lamps += CalcSpotLight(spotLights[i], norm, FragPos, viewDir);
        }

        result = result * lamps;
        FragColor = vec4(result, 1.0);  
    }
    else{
        FragColor = vec4(result, 1.0);
    }
}