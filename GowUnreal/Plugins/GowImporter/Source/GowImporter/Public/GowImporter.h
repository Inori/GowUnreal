// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "GowSceneBuilder.h"

class FGowImporterModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void PluginButtonClicked();

private:

	void RegisterMenus();

private:
	TSharedPtr<class FUICommandList> PluginCommands;
	FRunnableThread*                 BuilderThread;
	GowSceneBuilder*                 SceneBuilder;
};
