#version 460 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inUV0;
layout(location = 4) in vec2 inUV1;
layout(location = 5) in vec4 inColor0;
layout(location = 6) in uvec4 inJoints0;
layout(location = 7) in vec4 inWeights0;

layout(set = 0, binding = 0) uniform SceneUBO {
	mat4 Projection;
	mat4 View;
	mat4 ViewProjection;
	vec3 ViewPosition;
	vec3 LightPosition;
} Scene;

layout(set = 1, binding = 0, std430) readonly buffer SkinSSBO {
	mat4 JointMatrices[];
} Skin;

layout(push_constant) uniform PushConstant {
	mat4 Node;
	bool Skinned;
} PC;

struct VertexOut {
	vec3 WorldPos;
	vec2 UV0;
	vec2 UV1;
	vec4 Color0;
	mat3 NormalMat;
};

layout(location = 0) out VertexOut Out;

void main() {
	mat4 model;
	if (PC.Skinned) {
		mat4 skinMat =
			inWeights0.x * Skin.JointMatrices[inJoints0.x] +
			inWeights0.y * Skin.JointMatrices[inJoints0.y] +
			inWeights0.z * Skin.JointMatrices[inJoints0.z] +
			inWeights0.w * Skin.JointMatrices[inJoints0.w];
		model = PC.Node * skinMat;
	} else {
		model = PC.Node;
	}

	vec4 locPos = model * vec4(inPosition, 1.0f);
	mat3 normalMatrix = mat3(model);
	vec3 T = normalize(normalMatrix * inTangent.xyz);
	vec3 B = normalize(normalMatrix * (cross(inNormal.xyz, inTangent.xyz) * inTangent.w));
	vec3 N = normalize(normalMatrix * inNormal);

	Out.WorldPos = locPos.xyz / locPos.w;
	Out.UV0 = inUV0;
	Out.UV1 = inUV1;
	Out.Color0 = inColor0;
	Out.NormalMat = mat3(T, B, N);

	gl_Position = Scene.ViewProjection * vec4(Out.WorldPos, 1.0);
}
