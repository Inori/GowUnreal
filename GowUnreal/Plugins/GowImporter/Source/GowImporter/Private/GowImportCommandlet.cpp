#include "GowImportCommandlet.h"
#include "GowImporterCommon.h"
#include "AssetRegistryModule.h"
#include "GowInterface.h"
#include "UObject/SavePackage.h"
#include "RawMesh/Public/RawMesh.h"
#include "Engine/StaticMesh.h"
#include "PackageHelperFunctions.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Math/Matrix.h"
#include "Math/TransformNonVectorized.h"
#include "StaticMeshAttributes.h"
#include "Misc/FileHelper.h"
#include "GowTextureRef.h"

#include <glm.hpp>
#include <gtc/constants.hpp>
#include <gtc/matrix_access.hpp>
#include <gtx/euler_angles.hpp>

#define _MANAGED 0
#define _M_CEE 0

#define WIN32_LEAN_AND_MEAN
#include <DirectXTex.h>
#undef WIN32_LEAN_AND_MEAN

#ifdef UpdateResource
#undef UpdateResource
#endif

#pragma optimize("", off)

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
			auto Package = CreateAssetPackage(res);
			if (Package == nullptr)
			{
				LOG_DEBUG("Create package failed.");
				continue;
			}

			auto Mesh = CreateMesh(Package, res);

            CreateInstances(Package, Mesh, res);

			CreateTextures(Package, res);

            SavePackage(Package);
		}

    } while (false);

    return 0;
}

UPackage* UGowImportCommandlet::CreateAssetPackage(const GowResourceObject& obj)
{
	// Create Package
	FString ObjectName  = FString(obj.name.c_str());
	FString PackagePath = FString("/Game/Gow/");
	FString AssetPath   = PackagePath + ObjectName;

	UPackage* Package = CreatePackage(*AssetPath);
	Package->FullyLoad();
	return Package;
}

void UGowImportCommandlet::SavePackage(UPackage* Package)
{
	Package->MarkPackageDirty();

    FString FolderName      = Package->GetName();
	FString PackageFileName = FPackageName::LongPackageNameToFilename(FolderName, FPackageName::GetAssetPackageExtension());

	SavePackageHelper(Package, PackageFileName);
	LOG_DEBUG("Package saved: %s", *PackageFileName);
}

UStaticMesh* UGowImportCommandlet::CreateMesh(UPackage* Package, const GowResourceObject& obj)
{
    // Object Details
	auto    PackagePath = Package->GetName();
	auto    PackageName = FPaths::GetBaseFilename(PackagePath);
	FString ObjectName  = FString::Printf(TEXT("SM_%s"), *PackageName);

    FRawMesh RawMesh = {};
    
	auto convertPos = [](const glm::vec3& in) 
	{
		return FVector3f(in.x, in.z, in.y);
	};

    // Vertex
    for (const auto& pos : obj.position)
    {		
		// The original vertices coordinate is Y-up-Right-hand,
		// we need to convert it to Z-up-Left-hand,
		// so swap Y and Z
		RawMesh.VertexPositions.Add(convertPos(pos));
    }
    // Index
    for (const auto idx : obj.indices)
    {
		RawMesh.WedgeIndices.Add(idx);
    }

	// Normal
	size_t nFaces         = RawMesh.WedgeIndices.Num() / 3;
	RawMesh.WedgeTangentZ = ComputeNormalsWeightedByAngle(RawMesh.WedgeIndices,
														  nFaces,
														  RawMesh.VertexPositions,
														  false);
    
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

    // Create Static Mesh
	UStaticMesh* myStaticMesh = NewObject<UStaticMesh>(Package, FName(*ObjectName), RF_Public | RF_Standalone);

    myStaticMesh->GetStaticMaterials().Add(FStaticMaterial());
	myStaticMesh->GetSectionInfoMap().Set(0, 0, FMeshSectionInfo(0));

    // Saving mesh in the StaticMesh
	FStaticMeshSourceModel& SrcModel = myStaticMesh->AddSourceModel();
	// Model Configuration
	SrcModel.BuildSettings.bRecomputeNormals             = false;
	SrcModel.BuildSettings.bRecomputeTangents            = true;
	SrcModel.BuildSettings.bUseMikkTSpace                = false;
	SrcModel.BuildSettings.bGenerateLightmapUVs          = true;
	SrcModel.BuildSettings.bBuildReversedIndexBuffer     = false;
	SrcModel.BuildSettings.bUseFullPrecisionUVs          = false;
	SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
	SrcModel.SaveRawMesh(RawMesh);

	FMeshDescription* MeshDesc = SrcModel.GetOrCacheMeshDescription();
	const auto&              Triangles = MeshDesc->Triangles();
	for (const auto& ID : Triangles.GetElementIDs())
	{
		MeshDesc->ReverseTriangleFacing(ID);
	}
	SrcModel.CommitMeshDescription(false);

	// Processing the StaticMesh and Marking it as not saved
	myStaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
	myStaticMesh->CreateBodySetup();
	myStaticMesh->SetLightingGuid();
	myStaticMesh->PostEditChange();

	FAssetRegistryModule::AssetCreated(myStaticMesh);
	
    return myStaticMesh;
}

void UGowImportCommandlet::CreateInstances(UPackage* Package, UStaticMesh* Mesh, const GowResourceObject& obj)
{
	auto                           PackagePath = Package->GetName();
	auto                           PackageName = FPaths::GetBaseFilename(PackagePath);
	FString                        ObjectName  = FString::Printf(TEXT("ISC_%s"), *PackageName);
	UInstancedStaticMeshComponent* Component   = NewObject<UInstancedStaticMeshComponent>(Package, FName(*ObjectName), RF_Public | RF_Standalone);

	auto convertMatrix = [](const glm::mat4& in)
	{
		FMatrix out;
		for (int r = 0; r != 4; ++r)
		{
			for (int c = 0; c != 4; ++c)
			{
				out.M[r][c] = in[r][c];
			}
		}
		return out;
	};

	for (const auto& trs : obj.instances)
	{
		FMatrix modelView = convertMatrix(trs.modelView);
		FTransform ObjectTrasform(modelView);
		Component->AddInstance(ObjectTrasform, true);
	}
}

void UGowImportCommandlet::CreateTextures(UPackage* Package, const GowResourceObject& obj)
{
	auto PackagePath = Package->GetName();
	auto PackageName = FPaths::GetBaseFilename(PackagePath);

	for (const auto& TexFilename : obj.textures)
	{
		// Currently only support layer 0 material.
		if (TexFilename.find("_0_") == std::string::npos)
		{
			continue;
		}

		FString PropName = GetPropertyName(TexFilename).c_str();
		if (PropName.IsEmpty())
		{
			continue;
		}

		FString Key = FString(TexFilename.c_str());
		if (m_texMap.Contains(Key))
		{
			FString         ObjectName = FString::Printf(TEXT("TR_%s_%s"), *PackageName, *PropName);
			UGowTextureRef* TextureRef = NewObject<UGowTextureRef>(Package, *ObjectName, RF_Public | RF_Standalone);

			FString ExistingName = m_texMap[Key];
			TextureRef->SetReference(ExistingName);
		}
		else
		{
			FString     ObjectName = FString::Printf(TEXT("T_%s_%s"), *PackageName, *PropName);
			UTexture2D* NewTexture = NewObject<UTexture2D>(Package, *ObjectName, RF_Public | RF_Standalone);

			FillTexture(NewTexture, TexFilename);

			FString TexturePath = PackagePath / ObjectName;
			m_texMap.Add(Key, TexturePath);
		}
	}
}

bool UGowImportCommandlet::DecompressImage(const std::string& Filename, DirectX::ScratchImage& OutImages)
{
	bool ret = false;
	do
	{
		if (Filename.empty())
		{
			break;
		}

		TArray<uint8> DDSData;
		if (!FFileHelper::LoadFileToArray(DDSData, ANSI_TO_TCHAR(Filename.c_str())))
		{
			break;
		}

		DirectX::TexMetadata  Meta   = {};
		DirectX::ScratchImage OutDDS = {};
		if (DirectX::LoadFromDDSMemory(DDSData.GetData(), DDSData.Num(), DirectX::DDS_FLAGS_NONE, &Meta, OutDDS) != S_OK)
		{
			break;
		}

		if (DirectX::Decompress(OutDDS.GetImages(), OutDDS.GetImageCount(), Meta, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, OutImages) != S_OK)
		{
			break;
		}

		ret = true;
	}while(false);
	return ret;
}

void UGowImportCommandlet::FillTexture(UTexture2D* Texture, const std::string& SrcFilename)
{
	do 
	{
		if (!Texture)
		{
			break;
		}

		DirectX::ScratchImage BGRAImages = {};
		if (!DecompressImage(SrcFilename, BGRAImages))
		{
			break;
		}

		const auto& Meta = BGRAImages.GetMetadata();

		Texture->Source.Init(
			Meta.width,
			Meta.height,
			Meta.depth,
			Meta.mipLevels,
			TSF_BGRA8);

		if (SrcFilename.find("_normal") != std::string::npos)
		{
			Texture->LODGroup            = TEXTUREGROUP_WorldNormalMap;
			Texture->CompressionSettings = TC_Normalmap;
			Texture->SRGB                = 0;
		}
		else if (SrcFilename.find("_diffuse") == std::string::npos)
		{
			Texture->SRGB = 0;
		}

		for (size_t MipIndex = 0; MipIndex != Meta.mipLevels; ++MipIndex)
		{
			uint8* TextureData = Texture->Source.LockMip(MipIndex);

			auto Image = BGRAImages.GetImage(MipIndex, 0, 0);

			uint8* Src         = Image->pixels;
			uint8* Dst         = TextureData;
			size_t SrcRowPitch = Image->rowPitch;
			size_t DstRowPitch = 4 * Image->width;

			if (SrcRowPitch == DstRowPitch)
			{
				size_t ImageSize = Image->rowPitch * Image->height;
				FMemory::Memcpy(Dst, Src, ImageSize);
			}
			else
			{
				for (size_t row = 0; row != Image->height; ++row)
				{
					FMemory::Memcpy(Dst, Src, DstRowPitch);

					Dst += DstRowPitch;
					Src += SrcRowPitch;
				}
			}

			Texture->Source.UnlockMip(MipIndex);
		}

		Texture->UpdateResource();

	} while (false);

}

std::string UGowImportCommandlet::GetPropertyName(const std::string& Filename)
{
	std::string result;

	do 
	{
		auto start = Filename.rfind("_0__");
		if (start == std::string::npos)
		{
			break;
		}

		auto end = Filename.rfind(".");
		if (end == std::string::npos)
		{
			break;
		}

		auto len = end - start;
		result   = Filename.substr(start + 4, len - 4);
	} while (false);

	return result;
}

static FVector3f MultiplyAdd(const FVector3f& V1, const FVector3f& V2, const FVector3f& V3)
{
	return FVector3f(V1.X * V2.X + V3.X,
					 V1.Y * V2.Y + V3.Y,
					 V1.Z * V2.Z + V3.Z);
}

TArray<FVector3f> UGowImportCommandlet::ComputeNormalsWeightedByAngle(
	const TArray<uint32>&    indices,
	size_t                   nFaces,
	const TArray<FVector3f>& positions,
	bool                     cw)
{
	uint32 nVerts = positions.Num();

	TArray<FVector3f> vertNormals;
	vertNormals.SetNumZeroed(nVerts);

	for (size_t face = 0; face < nFaces; ++face)
	{
		uint32  i0 = indices[face * 3];
		uint32  i1 = indices[face * 3 + 1];
		uint32  i2 = indices[face * 3 + 2];

		if (i0 == -1 || i1 == -1 || i2 == -1)
		{
			continue;
		}
		
		if (i0 >= nVerts || i1 >= nVerts || i2 >= nVerts)
		{
			return TArray<FVector3f>();
		}

		const FVector3f p0 = positions[i0];
		const FVector3f p1 = positions[i1];
		const FVector3f p2 = positions[i2];

		FVector3f u = p1 - p0;
		FVector3f v = p2 - p0;
		u.Normalize();
		v.Normalize();

		FVector3f faceNormal = FVector3f::CrossProduct(u, v);
		faceNormal.Normalize();

		// Corner 0 -> 1 - 0, 2 - 0
		const FVector3f a   = u;
		const FVector3f b   = v;
		float           dot = FVector3f::DotProduct(a, b);
		dot                 = FMath::Clamp(dot, -1.0, 1.0);
		float     acos      = FMath::Acos(dot);
		FVector3f w0(acos, acos, acos);


		// Corner 1 -> 2 - 1, 0 - 1
		FVector3f c = p2 - p1;
		FVector3f d = p0 - p1;
		c.Normalize();
		d.Normalize();
		dot  = FVector3f::DotProduct(c, d);
		dot  = FMath::Clamp(dot, -1.0, 1.0);
		acos = FMath::Acos(dot);
		FVector3f w1(acos, acos, acos);

		// Corner 2 -> 0 - 2, 1 - 2
		FVector3f e = p0 - p2;
		FVector3f f = p1 - p2;
		e.Normalize();
		f.Normalize();
		dot  = FVector3f::DotProduct(e, f);
		dot  = FMath::Clamp(dot, -1.0, 1.0);
		acos = FMath::Acos(dot);
		FVector3f w2(acos, acos, acos);

		vertNormals[i0] = MultiplyAdd(faceNormal, w0, vertNormals[i0]);
		vertNormals[i1] = MultiplyAdd(faceNormal, w1, vertNormals[i1]);
		vertNormals[i2] = MultiplyAdd(faceNormal, w2, vertNormals[i2]);
	}

	// Store results
	size_t            nIndices = indices.Num();
	TArray<FVector3f> Result;
	Result.SetNum(nIndices);
	if (cw)
	{
		for (size_t i = 0; i < nIndices; ++i)
		{
			uint32 index = indices[i];
			vertNormals[index].Normalize();
			FVector3f n = -vertNormals[index];
			Result[i]   = n;
		}
	}
	else
	{
		for (size_t i = 0; i < nIndices; ++i)
		{
			uint32 index = indices[i];
			vertNormals[index].Normalize();
			FVector3f n = vertNormals[index];
			Result[i]   = n;
		}
	}

	return Result;
}

TTuple<FVector3f, FVector3f, FVector3f> UGowImportCommandlet::GetTriangleTangentsAndNormals(float ComparisonThreshold, TArrayView<const FVector3f> VertexPositions, TArrayView<const FVector2D> VertexUVs)
{
	float AdjustedComparisonThreshold = FMath::Max(ComparisonThreshold, MIN_flt);

	const FVector3f Position0  = VertexPositions[0];
	const FVector3f DPosition1 = VertexPositions[1] - Position0;
	const FVector3f DPosition2 = VertexPositions[2] - Position0;

	const FVector2f UV0  = FVector2f(VertexUVs[0]);
	const FVector2f DUV1 = FVector2f(VertexUVs[1]) - UV0;
	const FVector2f DUV2 = FVector2f(VertexUVs[2]) - UV0;

	// We have a left-handed coordinate system, but vertices in clockwise order
	FVector3f Normal = FVector3f::CrossProduct(DPosition2, DPosition1).GetSafeNormal(AdjustedComparisonThreshold);
	if (!Normal.IsNearlyZero(ComparisonThreshold))
	{
		FMatrix44f ParameterToLocal(
			DPosition1,
			DPosition2,
			Position0,
			FVector3f::ZeroVector);

		FMatrix44f ParameterToTexture(
			FPlane4f(DUV1.X, DUV1.Y, 0, 0),
			FPlane4f(DUV2.X, DUV2.Y, 0, 0),
			FPlane4f(UV0.X, UV0.Y, 1, 0),
			FPlane4f(0, 0, 0, 1));

		// Use InverseSlow to catch singular matrices.  Inverse can miss this sometimes.
		const FMatrix44f TextureToLocal = ParameterToTexture.Inverse() * ParameterToLocal;

		FVector3f Tangent  = TextureToLocal.TransformVector(FVector3f(1, 0, 0)).GetSafeNormal();
		FVector3f Binormal = TextureToLocal.TransformVector(FVector3f(0, 1, 0)).GetSafeNormal();
		FVector3f::CreateOrthonormalBasis(Tangent, Binormal, Normal);

		if (Tangent.IsNearlyZero() || Tangent.ContainsNaN() || Binormal.IsNearlyZero() || Binormal.ContainsNaN())
		{
			Tangent  = FVector3f::ZeroVector;
			Binormal = FVector3f::ZeroVector;
		}

		if (Normal.IsNearlyZero() || Normal.ContainsNaN())
		{
			Normal = FVector3f::ZeroVector;
		}

		return MakeTuple(Normal.GetSafeNormal(), Tangent.GetSafeNormal(), Binormal.GetSafeNormal());
	}
	else
	{
		// This will force a recompute of the normals and tangents
		return MakeTuple(FVector3f::ZeroVector, FVector3f::ZeroVector, FVector3f::ZeroVector);
	}
}

void UGowImportCommandlet::ComputeTriangleTangentsAndNormals(FMeshDescription& MeshDescription, float ComparisonThreshold /*= 0.0f*/)
{
	FStaticMeshAttributes Attributes(MeshDescription);
	Attributes.RegisterTriangleNormalAndTangentAttributes();

	// Check that the mesh description is compact
	const int32 NumTriangles = MeshDescription.Triangles().Num();
	if (MeshDescription.Triangles().GetArraySize() != NumTriangles)
	{
		FElementIDRemappings Remappings;
		MeshDescription.Compact(Remappings);
	}

	// Split work in batch to reduce call overhead
	const int32 BatchSize  = 8 * 1024;
	const int32 BatchCount = (NumTriangles + BatchSize - 1) / BatchSize;

	ParallelFor(BatchCount,
				[this, BatchSize, ComparisonThreshold, NumTriangles, &Attributes](int32 BatchIndex)
				{
					TArrayView<const FVector3f>         VertexPositions           = Attributes.GetVertexPositions().GetRawArray();
					TArrayView<const FVector2f>         VertexUVs                 = Attributes.GetVertexInstanceUVs().GetRawArray();
					TArrayView<const FVertexID>         TriangleVertexIDs         = Attributes.GetTriangleVertexIndices().GetRawArray();
					TArrayView<const FVertexInstanceID> TriangleVertexInstanceIDs = Attributes.GetTriangleVertexInstanceIndices().GetRawArray();

					TArrayView<FVector3f> TriangleNormals   = Attributes.GetTriangleNormals().GetRawArray();
					TArrayView<FVector3f> TriangleTangents  = Attributes.GetTriangleTangents().GetRawArray();
					TArrayView<FVector3f> TriangleBinormals = Attributes.GetTriangleBinormals().GetRawArray();

					int32 StartIndex = BatchIndex * BatchSize;
					int32 TriIndex   = StartIndex * 3;
					int32 EndIndex   = FMath::Min(StartIndex + BatchSize, NumTriangles);
					for (; StartIndex < EndIndex; ++StartIndex, TriIndex += 3)
					{
						FVector3f TriangleVertexPositions[3] = {
							VertexPositions[TriangleVertexIDs[TriIndex]],
							VertexPositions[TriangleVertexIDs[TriIndex + 1]],
							VertexPositions[TriangleVertexIDs[TriIndex + 2]]
						};

						FVector2D TriangleUVs[3] = {
							FVector2D(VertexUVs[TriangleVertexInstanceIDs[TriIndex]]),
							FVector2D(VertexUVs[TriangleVertexInstanceIDs[TriIndex + 1]]),
							FVector2D(VertexUVs[TriangleVertexInstanceIDs[TriIndex + 2]])
						};

						TTuple<FVector3f, FVector3f, FVector3f> Result = GetTriangleTangentsAndNormals(ComparisonThreshold, TriangleVertexPositions, TriangleUVs);
						TriangleNormals[StartIndex]                    = FVector3f(1.0, 0.0, 0.0);  // Result.Get<0>();
						TriangleTangents[StartIndex]                   = FVector3f(0.0, 1.0, 0.0);  // Result.Get<1>();
						TriangleBinormals[StartIndex]                  = FVector3f(0.0, 0.0, 1.0);  // Result.Get<2>();
					}
				});
}



#pragma optimize("", on)
