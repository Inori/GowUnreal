#pragma once

#define RENDERDOC_PLATFORM_WIN32
#include "renderdoc_replay.h"

#include "FbxBuilder.h"

#include <string>
#include <set>
#include <optional>
#include <map>

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
	MeshObject               extractMesh(const ActionDescription& act);
	std::vector<std::string> extractTexture(const ActionDescription& act);

	std::vector<MeshData>      getMeshAttributes(const ActionDescription& act);
	std::vector<uint32_t>      getMeshIndices(const MeshData& mesh);
	std::vector<MeshTransform> getMeshTransforms(const ActionDescription& act);
	MeshObject                 buildMeshObject(const ActionDescription& act);

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
	FbxBuilder           m_fbx;

	std::map<ResourceId, std::string> m_textureCache;
};

