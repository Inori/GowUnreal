#include "GowReplayer.h"

#define RENDERDOC_PLATFORM_WIN32
#include "renderdoc_replay.h"

#include <thread>
#include <chrono>

REPLAY_PROGRAM_MARKER();

GowReplayer::GowReplayer()
{
	initialize();
}

GowReplayer::~GowReplayer()
{
	shutdown();
}

void GowReplayer::replay(const std::string& capFile)
{
	captureLoad(capFile);



	captureUnload();
}

void GowReplayer::initialize()
{
	GlobalEnvironment env = {};
	RENDERDOC_InitialiseReplay(env, {});
}

void GowReplayer::shutdown()
{
	RENDERDOC_ShutdownReplay();
}

bool GowReplayer::captureLoad(const std::string& capFile)
{
	bool ret = false;
	do
	{
		m_cap = RENDERDOC_OpenCaptureFile();
		if (!m_cap)
		{
			break;
		}

		auto result = m_cap->OpenFile(capFile.c_str(), "rdc", nullptr);
		if (!result.OK())
		{
			break;
		}

		if (m_cap->LocalReplaySupport() != ReplaySupport::Supported)
		{
			break;
		}

		rdctie(result, m_player) = m_cap->OpenCapture({}, nullptr);
		if (!result.OK())
		{
			break;
		}

		ret  = true;
	}while(false);
	return ret;
}

void GowReplayer::captureUnload()
{
	if (m_player)
	{
		m_player->Shutdown();
		m_player = nullptr;
	}

	if (m_cap)
	{
		m_cap->Shutdown();
		m_cap = nullptr;
	}
}
