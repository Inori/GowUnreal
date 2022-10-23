#pragma once

#include <glm.hpp>
#define RENDERDOC_PLATFORM_WIN32
#include "renderdoc_replay.h"

#include <string>
#include <set>
#include <optional>
#include <map>

struct MeshTransform
{
    glm::vec3 translation;
    glm::vec3 rotation;
    glm::vec3 scaling;
};

struct GowMeshObject
{
    uint32_t               eid;
    std::string            name;
    std::vector<uint32_t>  indices;

    std::vector<glm::vec3> position;
    std::vector<glm::vec2> texcoord;
    std::vector<glm::vec4> tangent;
    std::vector<glm::vec4> normal;

    std::vector<MeshTransform> instances;
    std::vector<std::string>   textures;

    bool isValid();
};

struct MeshData : public MeshFormat
{
	uint64_t    indexOffset;
	std::string name;
};

class GowReplayer
{
	struct ResourceBuffer
	{
		ResourceId         id;
		ShaderConstantType format;
	};

	struct ResourceTexture
	{
		ResourceId  id;
		std::string name;
	};

public:
	GowReplayer();
	~GowReplayer();

	void replay(const std::string& capFile);

private:
	void initialize();
	void shutdown();

	bool captureLoad();
	void captureUnload();

	void processActions();
	void iterAction(const ActionDescription& act);

	void                     extractResource(const ActionDescription& act);
	GowMeshObject               extractMesh(const ActionDescription& act);
	std::vector<std::string> extractTexture(const ActionDescription& act);

	std::vector<MeshData>      getMeshAttributes(const ActionDescription& act);
	std::vector<uint32_t>      getMeshIndices(const MeshData& mesh);
	std::vector<MeshTransform> getMeshTransforms(const ActionDescription& act);
	GowMeshObject                 buildMeshObject(const ActionDescription& act);

	std::optional<ShaderVariable> getShaderConstantVariable(
		ShaderStage stage, const std::string& name);
	ResourceBuffer getShaderResourceBuffer(
		ShaderStage stage, const std::string& name);
	std::vector<ResourceTexture> getShaderResourceTextures(
		ShaderStage stage);

	std::vector<float> unpackData(
		const ResourceFormat& fmt,
		const uint8_t*        data);

	MeshTransform decomposeTransform(const glm::mat4& modelView);

	std::string getOutFilename();

	bool isBoneMesh();

private:
	std::string          m_capFilename;
	ICaptureFile*        m_cap    = nullptr;
	IReplayController*   m_player = nullptr;

	std::map<ResourceId, std::string> m_textureCache;
};

