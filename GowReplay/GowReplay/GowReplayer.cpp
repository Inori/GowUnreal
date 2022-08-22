#include "GowReplayer.h"
#include "Log.h"
#include "Tools.h"

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
#include <cmath>

#include "gtc/matrix_access.hpp"
#include "gtc/constants.hpp"
#include "gtx/euler_angles.hpp"
#include "half.hpp"


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
		if (!std::filesystem::is_regular_file(capFile))
		{
			LOG_ERR("rdc file not found: {}", capFile);
			break;
		}

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
	// only process DrawIndexedInstanced
	if (!((act.flags & ActionFlags::Drawcall) && 
		(act.flags & ActionFlags::Indexed) &&
		(act.flags & ActionFlags::Instanced)))
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

		mesh.vertexByteOffset = attr.byteOffset +
								vb.byteOffset +
								act.vertexOffset * vb.byteStride;
		mesh.format = attr.format;
		mesh.vertexResourceId = vb.resourceId;
		mesh.vertexByteStride = vb.byteStride;

		result.push_back(mesh);
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

std::vector<MeshTransform> GowReplayer::getMeshTransforms(const ActionDescription& act)
{
	std::vector<MeshTransform> instances = {};

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
	auto insBufferInfo = getShaderResourceBuffer(ShaderStage::Vertex, "instancingBuffer");
	LOG_ASSERT(insBufferInfo.id != ResourceId::Null(), "can not find instancingBuffer for EID {}", act.eventId);

	for (uint32_t instanceId = 0; instanceId != act.numInstances; ++instanceId)
	{
		uint32_t       instanceIndex = instanceId + instanceOffset;
		uint32_t       size          = insBufferInfo.format.arrayByteStride;
		uint32_t       offset        = size * instanceIndex;
		auto           instanceData  = m_player->GetBufferData(insBufferInfo.id, offset, size);
		const uint8_t* data          = instanceData.data();

		glm::mat4 insTransform(0);
		std::memcpy(&insTransform, data, insBufferInfo.format.members[0].type.arrayByteStride);
		insTransform[3][3] = 1.0;
		insTransform = glm::transpose(insTransform);

		// convert quant scale and bias into matrix
		glm::mat4 quant(1);
		quant[0][0] = quantScale[0];
		quant[1][1] = quantScale[1];
		quant[2][2] = quantScale[2];

		quant[3][0] += quantBias[0];
		quant[3][1] += quantBias[1];
		quant[3][2] += quantBias[2];
		
		// merge all transform
		glm::mat4 modelView = view * insTransform * quant;

		auto transform = decomposeTransform(modelView);
		instances.push_back(transform);
	}

	return instances;
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
		auto& buffer = vtxCache[attr.vertexResourceId];
		auto  offset = attr.vertexByteOffset;
		auto  stride = attr.vertexByteStride;
		auto  count  = buffer.size() / stride;

		for (size_t i = 0; i != count; ++i)
		{
			uint8_t* data  = &buffer[offset + i * stride];
			auto     value = unpackData(attr.format, data);

			if (attr.name == "POSITION")
			{
				LOG_ASSERT(value.size() == 3, "");
				glm::vec3 vtx(value[0], value[1], value[2]);
				mesh.position.push_back(vtx);
			}
			else if (attr.name == "TEXCOORD" || attr.name == "TEXCOORD0")
			{
				glm::vec2 texcoord(value[0], value[1]);
				mesh.texcoord.push_back(texcoord);
			}
			else if (attr.name == "NORMAL")
			{
				glm::vec4 normal(value[0], value[1], value[2], value.size() == 4 ? value[3] : 0.0f);
				mesh.normal.push_back(normal);
			}
			else if (attr.name == "TANGENT")
			{
				glm::vec4 tangent(value[0], value[1], value[2], value.size() == 4 ? value[3] : 1.0f);
				mesh.tangent.push_back(tangent);
			}
		}
	}

	LOG_ASSERT(mesh.texcoord.empty() || mesh.texcoord.size() == mesh.position.size(), "texcoord count not match");

	mesh.instances = getMeshTransforms(act);

	return mesh;
}

std::optional<ShaderVariable> GowReplayer::getShaderConstantVariable(
	ShaderStage stage, const std::string& name)
{
	std::optional<ShaderVariable> result;

	const auto& state = m_player->GetPipelineState();
	auto        pipe  = state.GetGraphicsPipelineObject();
	auto        entry = state.GetShaderEntryPoint(stage);
	auto        rf    = state.GetShaderReflection(stage);

	bool     found  = false;
	uint32_t varIdx = 0;
	for (const auto& cb : rf->constantBlocks)
	{
		if (cb.name.contains(name.c_str()))
		{
			found = true;
			break;
		}
		++varIdx;
	}

	LOG_ASSERT(found, "can not find {}.", name);

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

GowReplayer::ResourceBuffer GowReplayer::getShaderResourceBuffer(
	ShaderStage stage, const std::string& name)
{
	// TODO:
	// support read-write resource

	ResourceBuffer result = {};
	result.id = ResourceId::Null();

	const auto& state      = m_player->GetPipelineState();
	auto        rf         = state.GetShaderReflection(stage);
	const auto& resMapping = state.GetBindpointMapping(stage);

	rdcarray<BoundResourceArray> roBinds = state.GetReadOnlyResources(stage);
	for (const auto& res : rf->readOnlyResources)
	{
		if (!res.name.contains(name.c_str()))
		{
			continue;
		}

		Bindpoint bind = resMapping.readOnlyResources[res.bindPoint];

		if (!bind.used)
		{
			continue;
		}

		int32_t bindIdx = roBinds.indexOf(bind);

		if (bindIdx < 0)
			continue;

		BoundResourceArray& roBind = roBinds[bindIdx];

		LOG_ASSERT(bind.arraySize == 1, "array bind not supported.");

		if (roBind.resources.empty())
		{
			continue;
		}

		result.id     = roBind.resources[0].resourceId;
		result.format = res.variableType;

		break;
	}

	return result;
}

std::vector<float> GowReplayer::unpackData(
	const ResourceFormat& fmt,
	const uint8_t*        data)
{
	std::vector<float> result;

	LOG_ASSERT(fmt.BGRAOrder() == false, "BGRA order not supported.");

	auto fmtName = fmt.Name();

	// TODO:
	// this implementation is really ugly,
	// we should use a uniform way instead of support by name...
	if (fmtName == "R32G32B32_FLOAT" || fmtName == "R32G32_FLOAT" || fmtName == "R32G32B32A32_FLOAT")
	{
		LOG_ASSERT(fmt.compByteWidth == sizeof(float), "TODO");
		for (size_t i = 0; i != fmt.compCount; ++i)
		{
			result.push_back((reinterpret_cast<const float*>(data))[i]);
		}
	}
	else if (fmtName == "R16G16_UNORM")
	{
		LOG_ASSERT(fmt.compByteWidth == sizeof(uint16_t), "TODO");
		float divisor = float(std::exp2(fmt.compByteWidth * 8) - 1);
		for (size_t i = 0; i != fmt.compCount; ++i)
		{
			uint16_t val = (reinterpret_cast<const uint16_t*>(data))[i];
			result.push_back((float)val / divisor);
		}
	}
	else if (fmtName == "R8G8B8A8_UNORM")
	{
		LOG_ASSERT(fmt.compByteWidth == sizeof(uint8_t), "TODO");
		float divisor = float(std::exp2(fmt.compByteWidth * 8) - 1);
		for (size_t i = 0; i != fmt.compCount; ++i)
		{
			uint8_t val = (reinterpret_cast<const uint8_t*>(data))[i];
			result.push_back((float)val / divisor);
		}
	}
	else if (fmtName == "R16G16_SNORM")
	{
		LOG_ASSERT(fmt.compByteWidth == sizeof(int16_t), "TODO");
		float maxNeg  = -float(std::exp2(fmt.compByteWidth * 8)) / 2;
		float divisor = float(-(maxNeg - 1));
		for (size_t i = 0; i != fmt.compCount; ++i)
		{
			int16_t val  = (reinterpret_cast<const int16_t*>(data))[i];
			float   fval = (float)val == maxNeg ? (float)val : (float)val / divisor;
			result.push_back(fval);
		}
	}
	else if (fmtName == "R10G10B10A2_UNORM")
	{
		float divisor10 = float(std::exp2(10) - 1);
		float divisor2 = float(std::exp2(2) - 1);

		uint32_t value = *reinterpret_cast<const uint32_t*>(data);
		uint16_t r     = bit::extract(value, 9, 0);
		uint16_t g     = bit::extract(value, 19, 10);
		uint16_t b     = bit::extract(value, 29, 20);
		uint16_t a     = bit::extract(value, 31, 30);
		result.push_back((float)r / divisor10);
		result.push_back((float)g / divisor10);
		result.push_back((float)b / divisor10);
		result.push_back((float)a / divisor2);
	}
	else if (fmtName == "R16G16B16A16_UINT")
	{
		LOG_ASSERT(fmt.compByteWidth == sizeof(uint16_t), "TODO");
		for (size_t i = 0; i != fmt.compCount; ++i)
		{
			uint16_t val = (reinterpret_cast<const uint16_t*>(data))[i];
			result.push_back((float)val);
		}
	}
	else if (fmtName == "R16G16B16A16_FLOAT")
	{
		LOG_ASSERT(fmt.compByteWidth == sizeof(half_float::half), "TODO");
		for (size_t i = 0; i != fmt.compCount; ++i)
		{
			half_float::half val = (reinterpret_cast<const half_float::half*>(data))[i];
			result.push_back(val);
		}
	}
	else if (fmtName == "R8G8B8A8_UINT")
	{
		LOG_ASSERT(fmt.compByteWidth == sizeof(uint8_t), "TODO");
		for (size_t i = 0; i != fmt.compCount; ++i)
		{
			uint8_t val = (reinterpret_cast<const uint8_t*>(data))[i];
			result.push_back((float)val);
		}
	}
	else if (fmtName == "R32_UINT")
	{
		LOG_ASSERT(fmt.compByteWidth == sizeof(uint32_t), "TODO");
		for (size_t i = 0; i != fmt.compCount; ++i)
		{
			uint32_t val = (reinterpret_cast<const uint32_t*>(data))[i];
			result.push_back((float)val);
		}
	}
	else
	{
		LOG_ASSERT(false, "unsupported buffer format {}.", fmtName.c_str());
	}
	return result;
}

MeshTransform GowReplayer::decomposeTransform(const glm::mat4& modelView)
{
	MeshTransform result = {};
	
	result.translation.x = modelView[3][0];
	result.translation.y = modelView[3][1];
	result.translation.z = modelView[3][2];

	result.scaling.x = glm::length(glm::column(modelView, 0));
	result.scaling.y = glm::length(glm::column(modelView, 1));
	result.scaling.z = glm::length(glm::column(modelView, 2));

	const auto& scaling = result.scaling;
	// get upper 3x3 matrix
	glm::mat3 upper = glm::mat3(modelView);
	glm::column(upper, 0, glm::column(upper, 0) / scaling.x);
	glm::column(upper, 1, glm::column(upper, 1) / scaling.y);
	glm::column(upper, 2, glm::column(upper, 2) / scaling.z);
	
	// default rotation order in FBX SDK is of XYZ, R = Rx * Ry * Rz
	// so we need to rotate across Z then Y then X
	glm::mat4 rotation = glm::mat4(upper);
	glm::vec3 radians  = {};
	glm::extractEulerAngleZYX(rotation, radians.z, radians.y, radians.x);

	result.rotation = glm::degrees(radians);

	return result;
}

std::string GowReplayer::getOutFilename(const std::string& inFilename)
{
	std::filesystem::path inPath(inFilename);
	auto outPath = inPath.parent_path() / (inPath.stem().string() + std::string(".fbx"));
	return outPath.string();
}

bool GowReplayer::isBoneMesh()
{
	const auto& state = m_player->GetPipelineState();
	auto        rf    = state.GetShaderReflection(ShaderStage::Vertex);

	for (const auto& res : rf->readOnlyResources)
	{
		if (res.name.contains("bones") || res.name.contains("Bones"))
		{
			return true;
		}
	}

	return false;
}
