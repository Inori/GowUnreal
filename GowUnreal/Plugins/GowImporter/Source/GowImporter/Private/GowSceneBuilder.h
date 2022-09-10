#pragma once

#include "CoreMinimal.h"
#include "AssetRegistryModule.h"

struct GowObject
{
	UStaticMesh* Mesh;
	UInstancedStaticMeshComponent* Instances;
};

class GowSceneBuilder
{
public:
	GowSceneBuilder();
	~GowSceneBuilder();

	void Build();

private:
	GowObject PopulateGowObject(const TArray<FAssetData>& AssetList);
	void PlaceObjectInScene(const GowObject& Object);

private:

};

