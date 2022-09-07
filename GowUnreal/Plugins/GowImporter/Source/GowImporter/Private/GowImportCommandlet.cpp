#include "GowImportCommandlet.h"
#include "GowImporterCommon.h"
#include "AssetRegistryModule.h"
#include "GowInterface.h"
#include "UObject/SavePackage.h"
#include "Engine/StaticMesh.h"
#include "RawMesh.h"


#define LOG_DEBUG(format, ...) UE_LOG(LogGowImporterPlugin, Display, TEXT(format), __VA_ARGS__)

UGowImportCommandlet::UGowImportCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;

    m_gowApi = std::make_shared<GowInterface>();
}

UGowImportCommandlet::~UGowImportCommandlet()
{
}

int32 UGowImportCommandlet::Main(const FString& Params)
{
    UE_LOG(LogGowImporterPlugin, Display, TEXT("Gow Importer Start!"));

    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    do 
    {
        FString RdcPath = ParamsMap[TEXT("rdc")];
        if (RdcPath.IsEmpty())
        {
            break;
        }

        auto Resources = m_gowApi->extractResources(TCHAR_TO_UTF8(*RdcPath));

        for (const auto& res : Resources)
		{
			CreateMesh(res);
		}
        
        //FString AssetPath = TEXT("/Game/Gow/");
        //FString MeshName = TEXT("TestMesh");
        //AssetPath += MeshName;
        //UPackage* Package = CreatePackage(NULL, *AssetPath);
        //Package->FullyLoad();

        //UStaticMesh* NewStaticMesh = NewObject<UStaticMesh>(Package, FName(*MeshName), RF_Public | RF_Standalone);

        //Package->MarkPackageDirty();
        //FAssetRegistryModule::AssetCreated(NewStaticMesh);
        ////通过 asset 路径获取包中文件名
        //FString PackageFileName = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());
        ////进行保存
        //bool bSaved = UPackage::SavePackage(Package, NewStaticMesh, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone, *PackageFileName, GError, nullptr, true, true, SAVE_NoError);


    } while (false);

    return 0;
}

UStaticMesh* UGowImportCommandlet::CreateMesh(const GowResourceObject& obj)
{
    // Object Details
    FString ObjectName = FString(obj.name.c_str());

    FRawMesh RawMesh = {};
    // Vertex
    for (const auto& pos : obj.position)
    {
		RawMesh.VertexPositions.Add(FVector3f(pos[0], pos[1], pos[2]));
    }
    // Index
    for (const auto idx : obj.indices)
    {
		RawMesh.WedgeIndices.Add(idx);
    }
    // Normal
    //for (const auto& n : obj.normal)
	//{
	//	RawMesh.WedgeTangentZ.Add(FVector(n[0], n[1], n[2]));
	//}
    // Tangent
	//for (const auto& t : obj.tangent)
	//{
	//	RawMesh.WedgeTangentX.Add(FVector(t[0], t[1], t[2]));
	//}
    
    // Texcoord
    for (const auto idx : obj.indices)
	{
		auto uv = obj.texcoord.size() ? obj.texcoord[idx] : glm::vec2(0.0, 0.0);
		RawMesh.WedgeTexCoords[0].Add(FVector2f(uv[0], uv[1]));
	}
    // Material
	uint32_t FaceCount = obj.indices.size() / 3;
    for (uint32_t i = 0; i != FaceCount; ++i)
    {
		RawMesh.FaceMaterialIndices.Add(0);
		RawMesh.FaceSmoothingMasks.Add(0xFFFFFFFF);  // Phong
    }

    // Create Package
	FString PackagePath     = FString("/Game/Gow/");
	FString AssetPath       = PackagePath + ObjectName;
	FString PackageFileName = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());

    UPackage* Package = CreatePackage(*AssetPath);
	Package->FullyLoad();

    // Create Static Mesh
	UStaticMesh* myStaticMesh = NewObject<UStaticMesh>(Package, FName(*ObjectName), RF_Public | RF_Standalone);

    myStaticMesh->GetStaticMaterials().Add(FStaticMaterial());
	myStaticMesh->GetSectionInfoMap().Set(0, 0, FMeshSectionInfo(0));

    if (myStaticMesh != NULL)
    {
        // Saving mesh in the StaticMesh
		FStaticMeshSourceModel& SrcModel = myStaticMesh->AddSourceModel();
        // Model Configuration
		SrcModel.BuildSettings.bRecomputeNormals             = true;
		SrcModel.BuildSettings.bRecomputeTangents            = true;
		SrcModel.BuildSettings.bUseMikkTSpace                = false;
		SrcModel.BuildSettings.bGenerateLightmapUVs          = true;
		SrcModel.BuildSettings.bBuildReversedIndexBuffer     = false;
		SrcModel.BuildSettings.bUseFullPrecisionUVs          = false;
		SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
		SrcModel.SaveRawMesh(RawMesh);

        // Assign the Materials to the Slots (optional

        //for (int32 MaterialID = 0; MaterialID < numberOfMaterials; MaterialID++)
		//{
		//	myStaticMesh->StaticMaterials.Add(Materials[MaterialID]);
		//	myStaticMesh->SectionInfoMap.Set(0, MaterialID, FMeshSectionInfo(MaterialID));
		//}

        // Processing the StaticMesh and Marking it as not saved
        myStaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
        myStaticMesh->CreateBodySetup();
        myStaticMesh->SetLightingGuid();
        myStaticMesh->PostEditChange();

        Package->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(myStaticMesh);

        FSavePackageArgs SaveArgs   = {};
		SaveArgs.TopLevelFlags      = EObjectFlags::RF_Public | EObjectFlags::RF_Standalone;
		SaveArgs.bForceByteSwapping = true;
		SaveArgs.SaveFlags          = SAVE_NoError;
		SaveArgs.FinalTimeStamp     = FDateTime::MinValue();

		UPackage::SavePackage(Package, myStaticMesh, *PackageFileName, SaveArgs);

        LOG_DEBUG("StaticMesh saved: %s", *ObjectName);
    };

    return myStaticMesh;
}
