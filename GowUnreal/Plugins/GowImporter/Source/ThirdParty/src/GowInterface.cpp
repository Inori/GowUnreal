#include "GowInterface.h"
#include "GowReplayer.h"

bool GowResourceObject::isValid()
{
    return !position.empty();
}

GowInterface::GowInterface()
{
    m_player = std::make_shared<GowReplayer>();
}

GowInterface::~GowInterface()
{
}

std::vector<GowResourceObject> GowInterface::extractResources(const std::string& capFile)
{
	return m_player->replay(capFile);
}
