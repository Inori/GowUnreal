#include "GowTextureRef.h"
#include "Misc/FileHelper.h"


UGowTextureRef::UGowTextureRef()
{
}

UGowTextureRef::~UGowTextureRef()
{
}

void UGowTextureRef::SetReference(const FString& RefName)
{
	RefTexture = RefName;
}

const FString& UGowTextureRef::GetReference()
{
	return RefTexture;
}

