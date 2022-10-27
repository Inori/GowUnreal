#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "GowTextureRef.generated.h"


UCLASS()
class UGowTextureRef : public UObject
{
	GENERATED_BODY()
public:
	UGowTextureRef();
	virtual ~UGowTextureRef();

	void SetReference(const FString& RefName);

	const FString& GetReference();

private:

	UPROPERTY()
	FString RefTexture;
};
