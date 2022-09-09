// Copyright Epic Games, Inc. All Rights Reserved.

#include "GowUnreal.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, GowUnreal, "GowUnreal" );

// this export can be used to mark a program as a 'replay' program which should not be captured.
// Any program used for such purpose must define and export this symbol in the main exe or one dll
// that will be loaded before renderdoc.dll is loaded.
extern "C" __declspec(dllexport) void renderdoc__replay__marker()
{
}
