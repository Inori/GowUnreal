#pragma 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "GowTextureRef.generated.h"

// A simple container for DDS binary file data.
// The class is used only in commandlet,
// we'll create real unreal texture and material
// objects in editor plugin code.

UCLASS()
class UGowTextureRef : public UObject
{
	GENERATED_BODY()
public:
	UGowTextureRef();
	virtual ~UGowTextureRef();

	void SetReference(const FString& RefName);

private:

	UPROPERTY()
	FString RefTexture;
};
