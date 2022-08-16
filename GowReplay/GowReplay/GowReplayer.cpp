#include "GowReplayer.h"
#include "Log.h"

template <>
rdcstr DoStringise(const uint32_t& el)
{
	static char tmp[16];
	memset(tmp, 0, sizeof(tmp));
	_snprintf_s(tmp, 15, "%u", el);
	return tmp;
}

#include "pipestate.inl"
#include "renderdoc_tostr.inl"

#include <thread>
#include <chrono>
#include <filesystem>
#include <map>

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

	auto outFilename = getOutFilename(capFile);
	m_fbx.build(outFilename);
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
	m_player->AddFakeMarkers();

	const auto& actionList = m_player->GetRootActions();
	for (const auto& act : actionList)
	{
		iterAction(act);
	}
}

void GowReplayer::iterAction(const ActionDescription& act)
{
	std::string name = act.GetName(m_player->GetStructuredFile()).c_str();
	LOG_DEBUG("Process EID: {} Name: {}", act.eventId, name);

	if (act.IsFakeMarker())
	{
		// Do not process depth only pass
		if (name.find("Depth-only") != std::string::npos)
		{
			return;
		}
	}

	extractResource(act);

	for (const auto& child : act.children)
	{
		iterAction(child);
	}
}

void GowReplayer::extractResource(const ActionDescription& act)
{
	if (act.IsFakeMarker())
	{
		return;
	}

	extractMesh(act);
}

void GowReplayer::extractMesh(const ActionDescription& act)
{
	if (!(act.flags & ActionFlags::Drawcall))
	{
		return;
	}

	m_player->SetFrameEvent(act.eventId, true);

	LOG_TRACE("Add mesh from event {}", act.eventId);
	auto mesh = buildMeshObject(act);
	if (mesh.isValid())
	{
		m_fbx.addMesh(mesh);
	}
}

std::vector<MeshData> GowReplayer::getMeshAttributes(const ActionDescription& act)
{
	std::vector<MeshData> result;

	const auto& state = m_player->GetPipelineState();

	auto ib    = state.GetIBuffer();
	auto vbs   = state.GetVBuffers();
	auto attrs = state.GetVertexInputs();

	for (const auto& attr : attrs)
	{
		if (attr.perInstance)
		{
			LOG_WARN("instance attribute not handled: {}", attr.name.c_str());
			continue;
		}

		MeshData mesh;
		mesh.name = attr.name.c_str();

		// indices
		mesh.indexResourceId = act.flags & ActionFlags::Indexed
								   ? ib.resourceId
								   : ResourceId::Null();
		mesh.indexByteOffset = ib.byteOffset;
		mesh.indexByteStride = ib.byteStride;
		mesh.baseVertex      = act.baseVertex;
		mesh.indexOffset     = act.indexOffset;
		mesh.numIndices      = act.numIndices;

		// vertices
		const auto& vb = vbs[attr.vertexBuffer];

		// skip already processed mesh
		if (m_resourceSet.find(vb.resourceId) != m_resourceSet.end())
		{
			continue;
		}

		mesh.vertexByteOffset = attr.byteOffset +
								vb.byteOffset +
								act.vertexOffset * vb.byteStride;
		mesh.format = attr.format;
		mesh.vertexResourceId = vb.resourceId;
		mesh.vertexByteStride = vb.byteStride;


		result.push_back(mesh);
		m_resourceSet.insert(vb.resourceId);
	}
	return result;
}

std::vector<uint32_t> GowReplayer::getMeshIndices(const MeshData& mesh)
{
	std::vector<uint32_t> result(mesh.numIndices);

	if (mesh.indexResourceId != ResourceId::Null())
	{
		auto ibData = m_player->GetBufferData(mesh.indexResourceId, mesh.indexByteOffset, 0);
		auto offset = mesh.indexOffset * mesh.indexByteStride;
		const void* data   = ibData.data() + offset;
		std::generate(result.begin(), result.end(), [&, n = 0]() mutable
		{ 
			if (mesh.indexByteStride == 2)
			{
				return (uint32_t)((const uint16_t*)data)[n++];
			}
			else if (mesh.indexByteStride == 4)
			{
				return ((const uint32_t*)data)[n++];
			}
			else
			{
				LOG_ASSERT(false, "unsupported index stride {}", mesh.indexByteStride);
				return 0u;
			}
		});
	}
	else
	{
		std::generate(result.begin(), result.end(), [n = 0]() mutable
					  { return n++; });
	}
	return result;
}

MeshTransform GowReplayer::getMeshTransform(const ActionDescription& act)
{
	MeshTransform result = {};

	auto        targets = m_player->GetDisassemblyTargets(true);
	const auto& format  = targets[0];  // should be DXBC
	
	// find ModelView matrix

	auto viewData = getShaderConstantVariable(ShaderStage::Vertex, "viewData");
	LOG_ASSERT(viewData.has_value(), "can not find viewData for EID {}.", act.eventId);

	glm::mat4 view = {};
	for (const auto& field : viewData->members)
	{
		if (field.name != "view")
		{
			continue;
		}

		for (uint8_t r = 0; r != field.rows; ++r)
		{
			for (uint8_t c = 0; c != field.columns; ++c)
			{
				view[r][c] = field.value.f32v[r * field.columns + c];
			}
		}

		break;
	}

	// find model data

	auto modelData = getShaderConstantVariable(ShaderStage::Vertex, "modelData");
	LOG_ASSERT(modelData.has_value(), "can not find modelData for EID {}.", act.eventId);

	int       instanceOffset = 0;
	glm::vec3 quantScale     = { 1.0, 1.0, 1.0 };
	glm::vec3 quantBias      = { 0.0, 0.0, 0.0 };
	for (const auto& field : modelData->members)
	{
		if (field.name == "instanceOffset")
		{
			instanceOffset = field.value.s32v[0];
			continue;
		}

		if (field.name == "quantScale")
		{
			for (uint8_t c = 0; c != field.columns; ++c)
			{
				quantScale[c] = field.value.f32v[c];
			}
			continue;
		}

		if (field.name == "quantBias")
		{
			for (uint8_t c = 0; c != field.columns; ++c)
			{
				quantBias[c] = field.value.f32v[c];
			}
			continue;
		}
	}

	// find instance data

	
	return result;
}

MeshObject GowReplayer::buildMeshObject(const ActionDescription& act)
{
	MeshObject mesh;

	auto meshAttrs = getMeshAttributes(act);
	if (meshAttrs.empty())
	{
		// in case the mesh has already been added.
		return mesh;
	}

	mesh.eid     = act.eventId;
	mesh.name    = fmt::format("EID_{}", act.eventId);
	mesh.indices = getMeshIndices(meshAttrs.front());

	// cache the vertex buffer
	std::map<ResourceId, bytebuf> vtxCache;
	for (const auto& attr : meshAttrs)
	{
		vtxCache[attr.vertexResourceId] = m_player->GetBufferData(attr.vertexResourceId, 0, 0);
	}

	for (const auto& attr : meshAttrs)
	{
		if (attr.name != "POSITION")
		{
			continue;
		}

		auto& buffer = vtxCache[attr.vertexResourceId];
		auto  offset = attr.vertexByteOffset;
		auto  stride = attr.vertexByteStride;
		auto  count  = buffer.size() / stride;
		for (size_t i = 0; i != count; ++i)
		{
			uint8_t* data  = &buffer[offset + i * stride];
			auto     value = unpackData(attr.format, data);

			glm::vec3 vtx(value[0], value[1], value[2]);
			mesh.position.push_back(vtx);
		}
	}

	mesh.transform = getMeshTransform(act);

	return mesh;
}

std::optional<ShaderVariable> GowReplayer::getShaderConstantVariable(
	ShaderStage stage, const std::string& varName)
{
	std::optional<ShaderVariable> result;

	const auto& state = m_player->GetPipelineState();
	auto        pipe  = state.GetGraphicsPipelineObject();
	auto        entry = state.GetShaderEntryPoint(stage);
	auto        rf    = state.GetShaderReflection(stage);

	auto roResource = state.GetReadOnlyResources(stage);
	auto rwResource = state.GetReadWriteResources(stage);
	const auto& bindMapping = state.GetBindpointMapping(stage);

	bool     found  = false;
	uint32_t varIdx = 0;
	for (const auto& cb : rf->constantBlocks)
	{
		if (cb.name.contains(varName.c_str()))
		{
			found = true;
			break;
		}
		++varIdx;
	}

	LOG_ASSERT(found, "can not find {}.", varName);

	auto varBuffer = state.GetConstantBuffer(stage, varIdx, 0);
	auto varList   = m_player->GetCBufferVariableContents(pipe,
														  rf->resourceId,
														  stage,
														  entry,
														  varIdx,
														  varBuffer.resourceId,
														  0,
														  0);
	LOG_ASSERT(varList.size() == 1, "variable array not supported.");
	result = varList[0];
	return result;
}

std::vector<float> GowReplayer::unpackData(
	const ResourceFormat& fmt,
	const uint8_t*        data)
{
	std::vector<float> result;
	if (fmt.Special())
	{
		LOG_WARN("TODO: support packed formats.");
	}

	switch (fmt.compType)
	{
		case CompType::Float:
		{
			LOG_ASSERT(fmt.compByteWidth == sizeof(float), "TODO");
			for (size_t i = 0; i != fmt.compCount; ++i)
			{
				result.push_back((reinterpret_cast<const float*>(data))[i]);
			}
		}
			break;
		default:
			LOG_ASSERT(false, "TODO: not supported component type.");
			break;
	}
	return result;
}

std::string GowReplayer::getOutFilename(const std::string& inFilename)
{
	std::filesystem::path inPath(inFilename);
	auto outPath = inPath.parent_path() / (inPath.stem().string() + std::string(".fbx"));
	return outPath.string();
}
