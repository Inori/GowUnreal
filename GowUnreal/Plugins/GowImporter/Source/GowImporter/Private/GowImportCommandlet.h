#pragma once

#include <memory>
#include <glm.hpp>
#include "Commandlets/Commandlet.h"
#include "GowImportCommandlet.generated.h"

class GowInterface;
struct GowResourceObject;

namespace DirectX
{
	class ScratchImage;
}  // namespace DirectX

struct GowMeshObject
{
	UStaticMesh*       Mesh;
	TArray<FTransform> Instances;
};


UCLASS()
class UGowImportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGowImportCommandlet();
	virtual ~UGowImportCommandlet();

	int32 Main(const FString& Params) override;

private:
	UPackage* CreateAssetPackage(const GowResourceObject& obj);
	void      SavePackage(UPackage* Package);

	UStaticMesh* CreateMesh(UPackage* Package, const GowResourceObject& obj);
	void         CreateInstances(UPackage* Package, UStaticMesh* Mesh, const GowResourceObject& obj);
	void         CreateTextures(UPackage* Package, const GowResourceObject& obj);

	bool DecompressImage(const std::string& Filename, DirectX::ScratchImage& OutImages);
	void FillTexture(UTexture2D* Texture, const std::string& SrcFilename);

	std::string GetPropertyName(const std::string& SlotName);

	TArray<FVector3f> ComputeNormalsWeightedByAngle(
		const TArray<uint32>&    indices,
		size_t                   nFaces,
		const TArray<FVector3f>& positions,
		bool                     cw);

	void ComputeTriangleTangentsAndNormals(
		FMeshDescription& MeshDescription,
		float             ComparisonThreshold = 0.0f);

	TTuple<FVector3f, FVector3f, FVector3f>
	GetTriangleTangentsAndNormals(
		float                       ComparisonThreshold,
		TArrayView<const FVector3f> VertexPositions,
		TArrayView<const FVector2D> VertexUVs);

private:
	std::shared_ptr<GowInterface> m_gowApi;
	TMap<FString, FString>        m_texMap;
};

