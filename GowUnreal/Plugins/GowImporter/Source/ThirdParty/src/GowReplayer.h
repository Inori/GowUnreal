#pragma once

#include "GowInterface.h"

#define RENDERDOC_PLATFORM_WIN32
#include "renderdoc_replay.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

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

	std::vector<GowResourceObject> replay(const std::string& capFile);

private:
	void initialize();
	void shutdown();

	bool captureLoad();
	void captureUnload();

	void processActions();
	void iterAction(const ActionDescription& act);

	void                     extractResource(const ActionDescription& act);
	GowResourceObject        extractMesh(const ActionDescription& act);
	std::vector<std::string> extractTexture(const ActionDescription& act);

	std::vector<MeshData>      getMeshAttributes(const ActionDescription& act);
	std::vector<uint32_t>      getMeshIndices(const MeshData& mesh);
	std::vector<MeshTransform> getMeshTransforms(const ActionDescription& act);
	GowResourceObject          buildMeshObject(const ActionDescription& act);

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
	uint32_t    getVertexCount(
		const std::vector<uint32_t>& indices, uint32_t baseVertex);

	bool isBoneMesh();

private:
	std::string        m_capFilename;
	ICaptureFile*      m_cap    = nullptr;
	IReplayController* m_player = nullptr;

	std::map<ResourceId, std::string> m_textureCache;
	std::vector<GowResourceObject>    m_resources;
};
