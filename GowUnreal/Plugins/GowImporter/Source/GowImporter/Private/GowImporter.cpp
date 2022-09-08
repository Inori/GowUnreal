// Copyright Epic Games, Inc. All Rights Reserved.

#include "GowImporter.h"
#include "GowImporterCommon.h"
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

	if (BuilderThread)
	{
		BuilderThread->Kill();
		delete BuilderThread;
	}

	if (SceneBuilder)
	{
		delete SceneBuilder;
	}
}

void FGowImporterModule::PluginButtonClicked()
{
	LOG_DEBUG("Start Gow builder thread");
	SceneBuilder  = new GowSceneBuilder();
	BuilderThread = FRunnableThread::Create(SceneBuilder, TEXT("GowBuilderThread"));
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