#pragma once

#define RENDERDOC_PLATFORM_WIN32
#include "renderdoc_replay.h"

#include "FbxBuilder.h"

#include <string>
#include <set>

struct MeshData : public MeshFormat
{
	uint64_t    indexOffset;
	std::string name;
};

class GowReplayer
{
public:
	GowReplayer();
	~GowReplayer();

	void replay(const std::string& capFile);

private:
	void initialize();
	void shutdown();

	bool captureLoad(const std::string& capFile);
	void captureUnload();

	void processActions();
	void iterAction(const ActionDescription& act);

	void extractResource(const ActionDescription& act);
	void extractMesh(const ActionDescription& act);

	std::vector<MeshData> getMeshAttributes(const ActionDescription& act);
	std::vector<uint32_t> getMeshIndices(const MeshData& mesh);
	MeshTransform         getMeshTransform(const ActionDescription& act);
	MeshObject            buildMeshObject(const ActionDescription& act);

	std::vector<float> unpackData(
		const ResourceFormat& fmt,
		const uint8_t*        data);

	std::string getOutFilename(const std::string& inFilename);

private:
	ICaptureFile*        m_cap    = nullptr;
	IReplayController*   m_player = nullptr;
	std::set<ResourceId> m_resourceSet;
	FbxBuilder           m_fbx;
};

