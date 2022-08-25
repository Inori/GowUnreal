#pragma once

#include "Commandlets/Commandlet.h"
#include "GowImporterCommandlet.generated.h"

UCLASS()
class UGowImporterCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGowImporterCommandlet();
	virtual ~UGowImporterCommandlet();

	int32 Main(const FString& Params) override;

private:

};

