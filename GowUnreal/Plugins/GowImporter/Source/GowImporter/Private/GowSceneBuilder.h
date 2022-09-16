#pragma once

#include "CoreMinimal.h"
#include "AssetRegistryModule.h"

struct GowObject
{
	UStaticMesh*                   Mesh;
	UInstancedStaticMeshComponent* InstancedComponent;
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

	FTransform ConvertTransform(const FTransform& TransRH);

private:

};

