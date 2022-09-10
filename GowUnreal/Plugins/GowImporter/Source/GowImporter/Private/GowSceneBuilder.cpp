#include "GowSceneBuilder.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"

#pragma optimize("", off)

GowSceneBuilder::GowSceneBuilder()
{

}

GowSceneBuilder::~GowSceneBuilder()
{
}

void GowSceneBuilder::Build()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData>    AssetData;
	AssetRegistryModule.Get().GetAssetsByPath("/Game/Gow/", AssetData);
	
	TMultiMap<FString, FAssetData> AssetMap;
	TSet<FString>                  PackageSet;
	for (const auto& Asset : AssetData)
	{
		auto PackageName = Asset.PackageName.ToString();

		AssetMap.AddUnique(PackageName, Asset);
		PackageSet.Add(PackageName);
	}

	for (const auto& PackageName : PackageSet)
	{
		TArray<FAssetData> Assets;

		AssetMap.MultiFind(PackageName, Assets);
		auto Object = PopulateGowObject(Assets);

		PlaceObjectInScene(Object);
	}
}

GowObject GowSceneBuilder::PopulateGowObject(const TArray<FAssetData>& AssetList)
{
	GowObject Object;

	auto FindAsset = [&AssetList](const FString& Name) -> UObject*
	{
		for (const auto& Asset : AssetList)
		{
			if (Asset.AssetName.ToString().Equals(Name))
			{
				return Asset.GetAsset();
			}
		}
		return nullptr;
	};

	auto MeshAsset = FindAsset(TEXT("Mesh"));
	Object.Mesh    = Cast<UStaticMesh>(MeshAsset);

	auto InsComponentAsset = FindAsset("InstancedComponent");
	Object.Instances       = Cast<UInstancedStaticMeshComponent>(InsComponentAsset);

	return Object;
}

void GowSceneBuilder::PlaceObjectInScene(const GowObject& Object)
{

}

#pragma optimize("", on)