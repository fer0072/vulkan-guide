#version 450
layout (location = 0) in vec4 vPosition_uvx;
layout (location = 1) in vec4 vNormal_uvy;
layout (location = 2) in vec4 vColor;

layout (location = 0) out vec3 outColor;

layout(set = 0, binding = 0) uniform SceneData{   
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	mat4 lightViewProj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
} sceneData;

struct ObjectData{
	mat4 model;
	vec4 spherebounds;
}; 

//all object matrices
layout(std140,set = 1, binding = 0) readonly buffer ObjectBuffer{   

	ObjectData objects[];
} objectBuffer;

//all object indices
layout(set = 2, binding = 0) readonly buffer InstanceObjectIDBuffer{   

	uint IDs[];
} instanceObjectIDBuffer;

void main() 
{	
	uint objectID = instanceObjectIDBuffer.IDs[gl_InstanceIndex];
	mat4 modelMatrix = objectBuffer.objects[objectID].model;
	mat4 transformMatrix = (sceneData.lightViewProj * modelMatrix);
	gl_Position = transformMatrix * vec4(vPosition_uvx.xyz, 1.0f);	
}
