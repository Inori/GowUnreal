#include "GowImporterCommands.h"

#define LOCTEXT_NAMESPACE "FGowImporterModule"

void FGowImporterCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "GowImporter", "Execute GowImporter action", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE