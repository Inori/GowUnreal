#pragma once

#include <glm.hpp>
#include <string>
#include <vector>
#include <memory>

class GowReplayer;

struct MeshTransform
{
	glm::mat4 modelView;

    glm::vec3 translation;
    glm::vec3 rotation;
    glm::vec3 scaling;
};

struct GowResourceObject
{
    uint32_t               eid;
    std::string            name;
	bool                   bone;
    std::vector<uint32_t>  indices;

    std::vector<glm::vec3> position;
    std::vector<glm::vec2> texcoord;
    std::vector<glm::vec4> tangent;
    std::vector<glm::vec4> normal;

    std::vector<MeshTransform> instances;
    std::vector<std::string>   textures;

    bool isValid();
};

class GowInterface
{
public:
    GowInterface();
    ~GowInterface();

    std::vector<GowResourceObject> extractResources(const std::string& capFile);

private:
    std::shared_ptr<GowReplayer> m_player;
};

