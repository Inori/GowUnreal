#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "GowImporterStyle.h"

class FGowImporterCommands : public TCommands<FGowImporterCommands>
{
public:
	FGowImporterCommands() :
		TCommands<FGowImporterCommands>(TEXT("GowImporter"), NSLOCTEXT("Contexts", "GowImporter", "GowImporter Plugin"), NAME_None, FGowImporterStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> PluginAction;
};