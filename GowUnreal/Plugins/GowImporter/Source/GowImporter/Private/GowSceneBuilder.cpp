#include "GowSceneBuilder.h"
#include "GowImporterCommon.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorModeManager.h"
#include "Engine/StaticMeshActor.h"

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

	GEditor->EditorUpdateComponents();
	GLevelEditorModeTools().MapChangeNotify();
}

GowObject GowSceneBuilder::PopulateGowObject(const TArray<FAssetData>& AssetList)
{
	GowObject Object;

	auto FindAsset = [&AssetList](const FString& Name) -> UObject*
	{
		for (const auto& Asset : AssetList)
		{
			if (Asset.AssetName.ToString().Contains(Name))
			{
				return Asset.GetAsset();
			}
		}
		return nullptr;
	};

	auto MeshAsset = FindAsset(TEXT("SM_"));
	Object.Mesh    = Cast<UStaticMesh>(MeshAsset);

	auto InsComponentAsset    = FindAsset("ISC_");
	Object.InstancedComponent = Cast<UInstancedStaticMeshComponent>(InsComponentAsset);

	return Object;
}

void GowSceneBuilder::PlaceObjectInScene(const GowObject& Object)
{
	auto PackageName = Object.Mesh->GetPackage()->GetName();
	auto MeshName    = FPaths::GetBaseFilename(PackageName);

	UWorld* CurrentWorld    = GEditor->GetEditorWorldContext().World();
	ULevel* CurrentLevel    = CurrentWorld->GetCurrentLevel();
	UClass* StaticMeshClass = AStaticMeshActor::StaticClass();

	auto InstancedComponent = Object.InstancedComponent;
	auto InstanceCount      = InstancedComponent->GetInstanceCount();
	for (uint32_t Index = 0; Index != InstanceCount; ++Index)
	{
		FTransform ObjectTrasform;
		InstancedComponent->GetInstanceTransform(Index, ObjectTrasform, true);

		AActor*           NewActorCreated = GEditor->AddActor(CurrentLevel, StaticMeshClass, ObjectTrasform, true, RF_Public | RF_Standalone | RF_Transactional);
		AStaticMeshActor* SmActor         = Cast<AStaticMeshActor>(NewActorCreated);

		// ID Name & Visible Name
		const FString InstanceName = FString::Printf(TEXT("%s_%d"), *MeshName, Index);
		SmActor->Rename(*InstanceName);
		SmActor->SetActorLabel(InstanceName);

		SmActor->GetStaticMeshComponent()->SetStaticMesh(Object.Mesh);
		SmActor->GetStaticMeshComponent()->RegisterComponentWithWorld(CurrentWorld);
		CurrentWorld->UpdateWorldComponents(true, true);
		SmActor->RerunConstructionScripts();
	}

	LOG_DEBUG("Place Mesh %s", *MeshName);
}

#pragma optimize("", on)