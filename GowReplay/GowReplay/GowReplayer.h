#pragma once

#include <string>

struct ICaptureFile;
struct IReplayController;

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

private:
	ICaptureFile*      m_cap        = nullptr;
	IReplayController* m_player = nullptr;
};

