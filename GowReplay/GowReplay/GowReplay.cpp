#include "GowReplay.h"

#define RENDERDOC_PLATFORM_WIN32
#include "renderdoc_replay.h"

GowReplay::GowReplay()
{
	initialize();
}

GowReplay::~GowReplay()
{
	shutdown();
}

void GowReplay::initialize()
{
	GlobalEnvironment env = {};
	RENDERDOC_InitialiseReplay(env, {});
}

void GowReplay::shutdown()
{
	RENDERDOC_ShutdownReplay();
}
