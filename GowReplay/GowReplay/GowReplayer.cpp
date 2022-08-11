#include "GowReplayer.h"
#include "Log.h"

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

	processActions();

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
		LOG_DEBUG("loading capture file: {}", capFile);

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

	if (!ret)
	{
		LOG_ERR("open capture failed: {}", capFile);
	}

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

void GowReplayer::processActions()
{
	const auto& actionList = m_player->GetRootActions();
	for (const auto& act : actionList)
	{
		iterAction(act);
	}
}

void GowReplayer::iterAction(const ActionDescription& act)
{
	auto name = act.GetName(m_player->GetStructuredFile()).c_str();
	LOG_DEBUG("EID: {} Name: {}", act.eventId, name);


	for (const auto& child : act.children)
	{
		iterAction(child);
	}
}
