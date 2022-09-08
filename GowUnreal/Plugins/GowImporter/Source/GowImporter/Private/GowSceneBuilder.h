#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"

class GowSceneBuilder : public FRunnable
{
public:
	GowSceneBuilder();
	~GowSceneBuilder();

	bool Init() override;

	void Stop() override;

	void Exit() override;

	uint32 Run() override;

private:
	bool bStop = false;
};

