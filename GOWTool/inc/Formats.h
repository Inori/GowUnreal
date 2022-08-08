#pragma once
#include "Mesh.h"

class MGDefinition
{
	uint16_t defCount;
	uint32_t* defOffsets;
public:
	vector<MeshInfo> ReadMG(std::stringstream& file);
	~MGDefinition();
};
class SmshDefinition
{
public:
	vector<MeshInfo> ReadSmsh(std::iostream& file);
};