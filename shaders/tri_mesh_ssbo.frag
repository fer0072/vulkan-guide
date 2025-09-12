#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform SceneData{   
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	mat4 lightViewProj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} sceneData;

layout(set = 1, binding = 0) uniform sampler2D allTextures[];

layout(set = 4, binding = 0) uniform GLTFMaterialData{   

	vec4 colorFactors;
	vec4 metal_rough_factors;
	int colorTexID;
	int metalRoughTexID;
} materialData;

layout(set = 5, binding = 0) uniform sampler2DShadow shadowSampler;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inShadowCoord;

layout (location = 0) out vec4 outFragColor;

struct SHCoefficients {
    vec3 l00, l1m1, l10, l11, l2m2, l2m1, l20, l21, l22;
};

const SHCoefficients grace = SHCoefficients(
    vec3( 0.3623915,  0.2624130,  0.2326261 ),
    vec3( 0.1759131,  0.1436266,  0.1260569 ),
    vec3(-0.0247311, -0.0101254, -0.0010745 ),
    vec3( 0.0346500,  0.0223184,  0.0101350 ),
    vec3( 0.0198140,  0.0144073,  0.0043987 ),
    vec3(-0.0469596, -0.0254485, -0.0117786 ),
    vec3(-0.0898667, -0.0760911, -0.0740964 ),
    vec3( 0.0050194,  0.0038841,  0.0001374 ),
    vec3(-0.0818750, -0.0321501,  0.0033399 )
);

vec3 calcIrradiance(vec3 nor) {
    const SHCoefficients c = grace;
    const float c1 = 0.429043;
    const float c2 = 0.511664;
    const float c3 = 0.743125;
    const float c4 = 0.886227;
    const float c5 = 0.247708;
    return (
        c1 * c.l22 * (nor.x * nor.x - nor.y * nor.y) +
        c3 * c.l20 * nor.z * nor.z +
        c4 * c.l00 -
        c5 * c.l20 +
        2.0 * c1 * c.l2m2 * nor.x * nor.y +
        2.0 * c1 * c.l21  * nor.x * nor.z +
        2.0 * c1 * c.l2m1 * nor.y * nor.z +
        2.0 * c2 * c.l11  * nor.x +
        2.0 * c2 * c.l1m1 * nor.y +
        2.0 * c2 * c.l10  * nor.z
    );
}

float textureProj(vec4 P, vec2 offset)
{
	float shadow = 1.0;
	vec4 shadowCoord = P / P.w;
	shadowCoord.st = shadowCoord.st * 0.5 + 0.5;
	
	if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0) 
	{
		vec3 sc = vec3(vec2(shadowCoord.st + offset),shadowCoord.z);
		shadow =  texture(shadowSampler, sc);		
	}
	return shadow;
}
// 9 imad (+ 6 iops with final shuffle)
uvec3 pcg3d(uvec3 v) {

    v = v * 1664525u + 1013904223u;

    v.x += v.y*v.z;
    v.y += v.z*v.x;
    v.z += v.x*v.y;

    v ^= v >> 16u;

    v.x += v.y*v.z;
    v.y += v.z*v.x;
    v.z += v.x*v.y;

    return v;
}

float filterPCF(vec4 sc)
{
	ivec2 texDim = textureSize(shadowSampler, 0);
	float scale = 2;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 2;

	vec2 s = gl_FragCoord.xy;
	
	uvec4 u = uvec4(s, uint(s.x) ^ uint(s.y), uint(s.x) + uint(s.y));
    vec3 rand = pcg3d(u.xyz);//vec3(1,0,0);//
	rand = normalize(rand);


	//sc.x += dx * rand.z;
    //sc.y += dy * rand.y;
	vec2 dirA = normalize(rand.xy);
	vec2 dirB = normalize(vec2(-dirA.y,dirA.x));
	
	dirA *= dx;
	dirB *= dy;
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc,dirA*x + dirB*y);
			count++;
		}	
	}
	return shadowFactor / count;

}
 
void main() 
{
    float lightAngle = clamp(dot(inNormal, -sceneData.sunlightDirection.xyz),0.f,1.f);
    float shadow = 1.0f;
    if(lightAngle > 0.01f)
    {
        shadow = mix(0.f,0.1f, filterPCF(inShadowCoord / inShadowCoord.w));
    }

	float lightValue = max(dot(inNormal, sceneData.sunlightDirection.xyz) * sceneData.sunlightDirection.w, 0.1f);

	vec3 irradiance = calcIrradiance(inNormal) * sceneData.ambientColor.xyz * sceneData.ambientColor.w; 

    int colorID = materialData.colorTexID;
    vec3 color = inColor * texture(allTextures[colorID],inUV).xyz;

	outFragColor = vec4(color * lightValue * shadow + color * irradiance.x * vec3(0.2f) ,1.0f);
}

