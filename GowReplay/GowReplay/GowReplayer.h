#pragma once

#define RENDERDOC_PLATFORM_WIN32
#include "renderdoc_replay.h"

#include <string>

class GowReplayer
{
public:
	GowReplayer();
	~GowReplayer();

	void replay(const std::string& capFile);

private:
	void initialize();
	void shutdown();

	bool captureLoad(const std::string& capFile);
	void captureUnload();

	void processActions();
	void iterAction(const ActionDescription& act);

private:
	ICaptureFile*      m_cap    = nullptr;
	IReplayController* m_player = nullptr;
};

