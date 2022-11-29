#pragma once
#include <vector>
#include <fbxsdk.h>
#include "Mesh.h"
#include "Rig.h"

class FbxSdkManager
{
public:
	FbxSdkManager();
	~FbxSdkManager();

    void writeFbx(
        const std::filesystem::path& path,
        const vector<RawMeshContainer>& expMeshes,
        const Rig& armature);

private:
	void initializeSdkObjects();
	void destroySdkObjects();
	bool saveScene(const char* pFilename, int pFileFormat = -1, bool pEmbedMedia = false);

	FbxNode* createMesh(const RawMeshContainer& rawMesh);

    void assignNormal(const RawMeshContainer& rawMesh, FbxMesh* mesh);
    void assignTexcoord(const RawMeshContainer& rawMesh, FbxMesh* mesh);
	void assignTexcoord(Vec2* texcoord, uint32_t count, FbxMesh* mesh);
	void assignTangent(const RawMeshContainer& rawMesh, FbxMesh* mesh);

private:
	FbxManager* m_manager = nullptr;
	FbxScene* m_scene = nullptr;
}; 

void writeFbx(
    const std::filesystem::path& path,
    const vector<RawMeshContainer>& expMeshes,
    const Rig& armature);