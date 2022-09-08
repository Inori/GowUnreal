// Copyright Epic Games, Inc. All Rights Reserved.

#include "GowImporter.h"
#include "GowImporterStyle.h"
#include "GowImporterCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FGowImporterModule"

void FGowImporterModule::StartupModule()
{
	FGowImporterStyle::Initialize();
	FGowImporterStyle::ReloadTextures();

	FGowImporterCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FGowImporterCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FGowImporterModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGowImporterModule::RegisterMenus));
}

void FGowImporterModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FGowImporterStyle::Shutdown();

	FGowImporterCommands::Unregister();
}

void FGowImporterModule::PluginButtonClicked()
{
	// Put your "OnButtonClicked" stuff here
	FText DialogText = FText::Format(
		LOCTEXT("PluginButtonDialogText", "Add code to {0} in {1} to override this button's actions"),
		FText::FromString(TEXT("FPLUGIN_NAMEModule::PluginButtonClicked()")),
		FText::FromString(TEXT("PLUGIN_NAME.cpp")));
	FMessageDialog::Open(EAppMsgType::Ok, DialogText);
}

void FGowImporterModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	//{
	//	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	//	{
	//		FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
	//		Section.AddMenuEntryWithCommandList(FGowImporterCommands::Get().PluginAction, PluginCommands);
	//	}
	//}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FGowImporterCommands::Get().PluginAction));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGowImporterModule, GowImporter)