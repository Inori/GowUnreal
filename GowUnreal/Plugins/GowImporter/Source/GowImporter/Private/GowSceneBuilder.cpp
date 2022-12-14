#include "GowSceneBuilder.h"
#include "GowImporterCommon.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorModeManager.h"
#include "Engine/StaticMeshActor.h"
#include "RawMesh/Public/RawMesh.h"
#include "StaticMeshAttributes.h"
#include "GowTextureRef.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/MaterialInstanceConstant.h"
#include "AssetToolsModule.h"

#include <glm.hpp>
#include <gtc/constants.hpp>
#include <gtc/matrix_access.hpp>
#include <gtx/euler_angles.hpp>

#pragma optimize("", off)

GowSceneBuilder::GowSceneBuilder()
{

}

GowSceneBuilder::~GowSceneBuilder()
{
}

void GowSceneBuilder::Build()
{
	InitMaterialTemplate();
	ObjectsToSync.Empty();

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

		if (Object.Mesh == nullptr || Object.InstancedComponent == nullptr)
		{
			continue;
		}

		PlaceObjectInScene(Object);
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync, true);
	GEditor->EditorUpdateComponents();
	GLevelEditorModeTools().MapChangeNotify();
}


UObject* GowSceneBuilder::FindAsset(const TArray<FAssetData>& AssetList, const FString& Name)
{
	for (const auto& Asset : AssetList)
	{
		if (Asset.AssetName.ToString().Contains(Name))
		{
			return Asset.GetAsset();
		}
	}
	return nullptr;
}

FString GowSceneBuilder::ObjectPathToName(const FString& ObjectPath)
{
	FString BaseName    = FPaths::GetCleanFilename(ObjectPath);
	FString PackageName = FPaths::GetPath(ObjectPath);
	return PackageName + "." + BaseName;
}

void GowSceneBuilder::InitMaterialTemplate()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry&       AssetRegistry       = AssetRegistryModule.Get();

	FString    TemplatePath = TEXT("/Game/GowMaterial/M_GowTemplate.M_GowTemplate");
	FAssetData Asset = AssetRegistry.GetAssetByObjectPath(*TemplatePath);
	if (Asset.IsValid())
	{
		GowMaterialTemplate = Cast<UMaterial>(Asset.GetAsset());
	}
}

UTexture* GowSceneBuilder::FindTexture(const TArray<FAssetData>& AssetList, const FString& Name)
{
	UTexture* Result = nullptr;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry&       AssetRegistry       = AssetRegistryModule.Get();

	UObject* AssetObject = FindAsset(AssetList, Name);
	if (AssetObject)
	{
		// The asset object can be either a Texture or a TextureRef
		UTexture*       Texture    = Cast<UTexture>(AssetObject);
		UGowTextureRef* TextureRef = Cast<UGowTextureRef>(AssetObject);
		if (Texture)
		{
			Result = Texture;
		}
		else
		{
			const FString& RefName    = TextureRef->GetReference();
			FString        ObjectName = ObjectPathToName(RefName);
			FAssetData     Asset      = AssetRegistry.GetAssetByObjectPath(*ObjectName);
			if (Asset.IsValid())
			{
				Result = Cast<UTexture>(Asset.GetAsset());
			}
		}
	}
	return Result;
}


GowObject GowSceneBuilder::PopulateGowObject(const TArray<FAssetData>& AssetList)
{
	GowObject Object = {};

	auto MeshAsset = FindAsset(AssetList, TEXT("SM_"));
	Object.Mesh    = Cast<UStaticMesh>(MeshAsset);

	auto InsComponentAsset    = FindAsset(AssetList, TEXT("ISC_"));
	Object.InstancedComponent = Cast<UInstancedStaticMeshComponent>(InsComponentAsset);

	Object.Diffuse = FindTexture(AssetList, TEXT("diffuse"));
	Object.Normal  = FindTexture(AssetList, TEXT("normal"));
	Object.Gloss   = FindTexture(AssetList, TEXT("gloss"));
	Object.Ao      = FindTexture(AssetList, TEXT("ao"));

	return Object;
}

void GowSceneBuilder::PlaceObjectInScene(const GowObject& Object)
{
	auto PackageName = Object.Mesh->GetPackage()->GetName();
	auto MeshName    = FPaths::GetBaseFilename(PackageName);

	UWorld* CurrentWorld    = GEditor->GetEditorWorldContext().World();
	ULevel* CurrentLevel    = CurrentWorld->GetCurrentLevel();
	UClass* StaticMeshClass = AStaticMeshActor::StaticClass();

	UMaterialInterface* Material = CreateOrGetMaterialInstance(Object);

	auto InstancedComponent = Object.InstancedComponent;
	auto InstanceCount      = InstancedComponent->GetInstanceCount();
	for (uint32_t Index = 0; Index != InstanceCount; ++Index)
	{
		FTransform ObjectTrasform = FTransform::Identity;
		InstancedComponent->GetInstanceTransform(Index, ObjectTrasform, true);

		FTransform UTransform = ConvertTransform(ObjectTrasform);

		AActor*           NewActorCreated = GEditor->AddActor(CurrentLevel, StaticMeshClass, UTransform, true, RF_Public | RF_Standalone | RF_Transactional);
		AStaticMeshActor* SmActor         = Cast<AStaticMeshActor>(NewActorCreated);
		// AddActor calls SpawnActor internally, which only accepts
		// location and rotation, so we need to set scaling manually.
		SmActor->SetActorScale3D(UTransform.GetScale3D());

		// ID Name & Visible Name
		const FString InstanceName = FString::Printf(TEXT("%s_%d"), *MeshName, Index);
		SmActor->Rename(*InstanceName);
		SmActor->SetActorLabel(InstanceName);

		UStaticMeshComponent* Component = SmActor->GetStaticMeshComponent();

		Component->SetStaticMesh(Object.Mesh);
		if (!Component->IsRegistered())
		{
			SmActor->GetStaticMeshComponent()->RegisterComponentWithWorld(CurrentWorld);
		}
		
		if (Material)
		{
			Component->SetMaterial(0, Material);
		}
		
		CurrentWorld->UpdateWorldComponents(true, true);
		SmActor->RerunConstructionScripts();
	}

	LOG_DEBUG("Place Mesh %s", *MeshName);
}

UMaterialInterface* GowSceneBuilder::CreateOrGetMaterialInstance(const GowObject& Object)
{
	UMaterialInterface* Result = nullptr;
	do 
	{
		if (!GowMaterialTemplate)
		{
			break;
		}

		if (!Object.Diffuse)
		{
			break;
		}

		FString Key = FPaths::GetBaseFilename(Object.Diffuse->GetPackage()->GetName());
		if (MaterialMap.Contains(Key))
		{
			Result = MaterialMap[Key];
		}
		else
		{
			UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
			Factory->InitialParent                       = GowMaterialTemplate;

			FString PackagePath  = FPackageName::GetLongPackagePath(GowMaterialTemplate->GetPackage()->GetName());
			FString ID           = FPaths::GetBaseFilename(Object.Mesh->GetPackage()->GetName());
			FString MaterialName = FString::Printf(TEXT("MI_%s"), *ID);

			FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
			UObject*           NewAsset         = AssetToolsModule.Get().CreateAsset(MaterialName, PackagePath, UMaterialInstanceConstant::StaticClass(), Factory);

			if (NewAsset)
			{
				ObjectsToSync.Add(NewAsset);
				Result = Cast<UMaterialInterface>(NewAsset);
				MaterialMap.Add(Key, Result);
			}

			UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(NewAsset);
			if (MIC)
			{
				MIC->SetTextureParameterValueEditorOnly(FMaterialParameterInfo("Diffuse"), Object.Diffuse);
				MIC->SetTextureParameterValueEditorOnly(FMaterialParameterInfo("Normal"), Object.Normal);
				MIC->SetTextureParameterValueEditorOnly(FMaterialParameterInfo("Gloss"), Object.Gloss);
				MIC->SetTextureParameterValueEditorOnly(FMaterialParameterInfo("Ao"), Object.Ao);

				MIC->PostEditChange();
			}
		}

	} while (false);
	return Result;
}

FTransform GowSceneBuilder::ConvertTransform(const FTransform& TransRH)
{
	// The code here is actually a copy from GowReplayer.cpp, to decompose TRS from matrix,
	// because I don't know how to store custom data in uaaset file.
	// For the reason why we need TRS components, that's because we need to convert 
	// God of war coordinate system to Unreal coordinate system.
	// I do think there are betters ways to do this, but I can't find any examples,
	// except this one:
	// https://stackoverflow.com/questions/57025469/how-to-convert-euler-rotations-from-one-coordinate-system-to-another-right-hand
	// Another way to do this is to apply model view matrix to every vertices
	// of the mesh which is the way FbxImporter in Unreal Engine use, and I think it's
	// not a good idea.

	auto ConvertMatrix = [](const FMatrix& In) 
	{
		glm::mat4 Out;
		for (int r = 0; r != 4; ++r)
		{
			for (int c = 0; c != 4; ++c)
			{
				Out[r][c] = In.M[r][c];
			}
		}
		return Out;
	};

	FMatrix UMat = TransRH.ToMatrixWithScale();
	glm::mat4 modelView = ConvertMatrix(UMat);

	// Gow/Maya/GLM transform, right hand
	glm::vec3 translation;
	translation.x = modelView[3][0];
	translation.y = modelView[3][1];
	translation.z = modelView[3][2];

	glm::vec3 scaling;
	scaling.x = glm::length(glm::column(modelView, 0));
	scaling.y = glm::length(glm::column(modelView, 1));
	scaling.z = glm::length(glm::column(modelView, 2));

	// get upper 3x3 matrix
	glm::mat3 upper = glm::mat3(modelView);
	glm::column(upper, 0, glm::column(upper, 0) / scaling.x);
	glm::column(upper, 1, glm::column(upper, 1) / scaling.y);
	glm::column(upper, 2, glm::column(upper, 2) / scaling.z);

	// default rotation order in FBX SDK is of XYZ, R = Rx * Ry * Rz
	// so we need to rotate across Z then Y then X
	glm::mat4 rotation = glm::mat4(upper);
	glm::vec3 radians  = {};
	glm::extractEulerAngleZYX(rotation, radians.z, radians.y, radians.x);
	glm::vec3 euler = glm::degrees(radians);

	// Unreal Transform, left hand
	float UniformScale = 500.0 / 5.5;

	// Translation
	FVector Position;
	// Position - Swap Z and Y axis
	Position.X = translation.x;
	Position.Y = translation.z;
	Position.Z = translation.y;

	// Quaternions - Convert Rotations from XSI to UE4
	FQuat qx(FVector(1, 0, 0), -radians.x);
	FQuat qz(FVector(0, 0, 1), -radians.y);
	FQuat qy(FVector(0, 1, 0), -radians.z);

	FQuat qu = qy * qz * qx;  // Change Rotation Order if necessary
	FRotator Rotation(qu);

	// Scale
	FVector Scale(scaling.x, scaling.y, scaling.z);  

	FTransform OutTransform(Rotation, Position * UniformScale, Scale * UniformScale);
	return OutTransform;
}


#pragma optimize("", on)