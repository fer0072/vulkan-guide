#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

layout (location = 0) in vec4 vPosition_uvx;
layout (location = 1) in vec4 vNormal_uvy;
layout (location = 2) in vec4 vColor;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
//layout(location = 3) out vec4 ShadowCoord;

layout(set = 0, binding = 0) uniform  SceneData{   

	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} sceneData;

layout(set = 0, binding = 1) uniform sampler2D allTextures[];

struct ObjectData{
	mat4 model;
	vec4 spherebounds;
	vec4 extents;
}; 

//all object matrices
layout(std140,set = 1, binding = 0) readonly buffer ObjectBuffer{   

	ObjectData objects[];
} objectBuffer;

struct GPUInstance {
	uint objectID;
	uint batchID;
};

//all object indices
layout(set = 1, binding = 1) readonly buffer InstanceBuffer{   

	GPUInstance Instances[];
} instanceBuffer;

layout(set = 2, binding = 0) uniform GLTFMaterialData{   

	vec4 colorFactors;
	vec4 metal_rough_factors;
	int colorTexID;
	int metalRoughTexID;
} materialData;

void main() 
{	
	uint index = instanceBuffer.Instances[gl_InstanceIndex].objectID;
	
	mat4 modelMatrix = objectBuffer.objects[index].model;
	gl_Position =  sceneData.viewproj * modelMatrix * vec4(vPosition_uvx.xyz, 1.0f);

	outNormal = (modelMatrix * vec4(vNormal_uvy.xyz, 0.f)).xyz;
	
	outColor = vColor.xyz * materialData.colorFactors.xyz;	

	outUV.x = vPosition_uvx.w;
	outUV.y = vNormal_uvy.w;
}
