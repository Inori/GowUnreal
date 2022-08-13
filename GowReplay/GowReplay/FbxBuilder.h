#pragma once

#include "fbxsdk.h"
#include "glm.hpp"

#include <vector>
#include <string>


struct MeshObject
{
	std::string            name;
	std::vector<uint32_t>  indices;
	std::vector<glm::vec3> position;
};


class FbxBuilder
{
public:
	FbxBuilder();
	~FbxBuilder();

	void addMesh(const MeshObject& mesh);

	void build(const std::string& filename);

private:
	FbxNode* createMesh(const MeshObject& mesh);

	void initializeSdkObjects();
	void destroySdkObjects();

private:
	FbxManager* m_manager = nullptr;
	FbxScene*   m_scene   = nullptr;
};

