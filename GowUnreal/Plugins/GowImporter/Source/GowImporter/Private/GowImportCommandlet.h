#pragma once

#include <memory>
#include "Commandlets/Commandlet.h"
#include "GowImportCommandlet.generated.h"

class GowInterface;
struct GowResourceObject;

struct MeshObject
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

private:
	std::shared_ptr<GowInterface> m_gowApi;
};

