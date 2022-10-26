#pragma once

#include "CoreMinimal.h"
#include "AssetRegistryModule.h"

struct GowObject
{
	UStaticMesh*                   Mesh;
	UInstancedStaticMeshComponent* InstancedComponent;

	UTexture* Diffuse;
	UTexture* Normal;
	UTexture* Gloss;
	UTexture* Ao;
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

	UObject*  FindAsset(const TArray<FAssetData>& AssetList, const FString& Name);
	UTexture* FindTexture(const TArray<FAssetData>& AssetList, const FString& Name);
	FString   ObjectPathToName(const FString& ObjectPath);

private:

};

