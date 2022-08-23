#pragma once

#include "fbxsdk.h"
#include "glm.hpp"

#include <vector>
#include <string>

struct MeshTransform
{
	glm::vec3 translation;
	glm::vec3 rotation;
	glm::vec3 scaling;

	bool operator==(const MeshTransform& other);
};

struct MeshObject
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


class FbxBuilder
{
public:
	FbxBuilder();
	~FbxBuilder();

	void addMesh(const MeshObject& mesh);

	void build(const std::string& filename);

private:
	FbxMesh*              createMesh(const MeshObject& mesh);
	std::vector<FbxNode*> createInstances(const MeshObject& info, FbxMesh* mesh);
	void                  assignNormal(const MeshObject& info, FbxMesh* mesh);
	void                  assignTexcoord(const MeshObject& info, FbxMesh* mesh);
	FbxSurfacePhong*      createMaterial(FbxNode* lNode);
	FbxFileTexture*       createTexture(const std::string& filename);
	void                  createMaterial(const MeshObject& info, const std::vector<FbxNode*>& nodeList);
	std::string           findTexturePath(const MeshObject& info, const std::string& name);

	void initializeSdkObjects();
	void destroySdkObjects();

private:
	FbxManager* m_manager = nullptr;
	FbxScene*   m_scene   = nullptr;
	std::string m_uvName  = "SharedUV";
};

