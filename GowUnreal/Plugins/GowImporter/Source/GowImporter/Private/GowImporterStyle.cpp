#include "GowImporterStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Slate/SlateGameResources.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FGowImporterStyle::StyleInstance = nullptr;

void FGowImporterStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FGowImporterStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FGowImporterStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("GowImporterStyle"));
	return StyleSetName;
}

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef<FSlateStyleSet> FGowImporterStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("GowImporterStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("GowImporter")->GetBaseDir() / TEXT("Resources"));

	Style->Set("GowImporter.PluginAction", new IMAGE_BRUSH_SVG(TEXT("PlaceholderButtonIcon"), Icon20x20));

	return Style;
}

void FGowImporterStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FGowImporterStyle::Get()
{
	return *StyleInstance;
}
