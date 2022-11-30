#pragma once
#include <vector>
#include <fbxsdk.h>
#include <glm.hpp>
#include "Mesh.h"
#include "Rig.h"

class FbxSdkManager
{
	struct MeshTransform
	{
        glm::vec3 translation;
        glm::vec3 rotation;
        glm::vec3 scaling;
	};
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

	void bindSkeleton(const RawMeshContainer& rawMesh, const Rig& armature, FbxNode* meshNode);
	std::vector<FbxNode*> createSkeleton(const Rig& armature);
	void linkSkeleton(const RawMeshContainer& rawMesh, const Rig& armature, FbxSkin* skin, FbxNode* skeleton);

	MeshTransform decomposeTransform(const glm::mat4& modelView);

private:
	FbxManager* m_manager = nullptr;
	FbxScene* m_scene = nullptr;
}; 

void writeFbx(
    const std::filesystem::path& path,
    const vector<RawMeshContainer>& expMeshes,
    const Rig& armature);