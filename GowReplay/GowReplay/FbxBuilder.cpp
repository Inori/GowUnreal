#include "FbxBuilder.h"
#include "Log.h"

#include <algorithm>
#include <filesystem>

#ifdef IOS_REF
#undef IOS_REF
#define IOS_REF (*(m_manager->GetIOSettings()))
#endif

namespace fs = std::filesystem;

bool MeshTransform::operator==(const MeshTransform& other)
{
	return translation == other.translation &&
		   rotation == other.rotation &&
		   scaling == other.scaling;
}

bool MeshObject::isValid()
{
	return !position.empty();
}

FbxBuilder::FbxBuilder()
{
	initializeSdkObjects();
}

FbxBuilder::~FbxBuilder()
{
	destroySdkObjects();
}

void FbxBuilder::addMesh(const MeshObject& mesh)
{
	auto lMesh     = createMesh(mesh);
	auto meshNodes = createInstances(mesh, lMesh);

	// UV should be assigned after node created
	assignTexcoord(mesh, lMesh);

	// Create material for every instance node
	createMaterial(mesh, meshNodes);

	// Add nodes to root.
	FbxNode* rootNode = m_scene->GetRootNode();
	for (const auto& node : meshNodes)
	{
		rootNode->AddChild(node);
	}
}

void FbxBuilder::build(const std::string& filename)
{
	int  lMajor, lMinor, lRevision;
	bool lStatus = true;
	int  lFileFormat = -1;
	bool pEmbedMedia = false;
	// Create an exporter.
	FbxExporter* lExporter = FbxExporter::Create(m_manager, "");

	if (lFileFormat < 0 || lFileFormat >= m_manager->GetIOPluginRegistry()->GetWriterFormatCount())
	{
		// Write in fall back format in less no ASCII format found
		lFileFormat = m_manager->GetIOPluginRegistry()->GetNativeWriterFormat();

		// Try to export in ASCII if possible
		int lFormatIndex, lFormatCount = m_manager->GetIOPluginRegistry()->GetWriterFormatCount();

		for (lFormatIndex = 0; lFormatIndex < lFormatCount; lFormatIndex++)
		{
			if (m_manager->GetIOPluginRegistry()->WriterIsFBX(lFormatIndex))
			{
				FbxString   lDesc  = m_manager->GetIOPluginRegistry()->GetWriterFormatDescription(lFormatIndex);
				//const char* lFormat = "ascii";
				const char* lFormat = "binary";
				if (lDesc.Find(lFormat) >= 0)
				{
					lFileFormat = lFormatIndex;
					break;
				}
			}
		}
	}

	// Set the export states. By default, the export states are always set to
	// true except for the option eEXPORT_TEXTURE_AS_EMBEDDED. The code below
	// shows how to change these states.
	IOS_REF.SetBoolProp(EXP_FBX_MATERIAL, true);
	IOS_REF.SetBoolProp(EXP_FBX_TEXTURE, true);
	IOS_REF.SetBoolProp(EXP_FBX_EMBEDDED, pEmbedMedia);
	IOS_REF.SetBoolProp(EXP_FBX_SHAPE, true);
	IOS_REF.SetBoolProp(EXP_FBX_GOBO, true);
	IOS_REF.SetBoolProp(EXP_FBX_ANIMATION, true);
	IOS_REF.SetBoolProp(EXP_FBX_GLOBAL_SETTINGS, true);

	// Initialize the exporter by providing a filename.
	if (lExporter->Initialize(filename.c_str(), lFileFormat, m_manager->GetIOSettings()) == false)
	{
		LOG_ERR("Call to FbxExporter::Initialize() failed.\n");
		LOG_ERR("Error returned: %s\n\n", lExporter->GetStatus().GetErrorString());
		return;
	}

	FbxManager::GetFileFormatVersion(lMajor, lMinor, lRevision);
	LOG_TRACE("FBX file format version {}.{}.{}\n\n", lMajor, lMinor, lRevision);

	// Export the scene.
	lStatus = lExporter->Export(m_scene);

	// Destroy the exporter.
	lExporter->Destroy();
}

FbxMesh* FbxBuilder::createMesh(const MeshObject& mesh)
{
	FbxMesh* lMesh = FbxMesh::Create(m_scene, "");

	// Create control points.
	lMesh->InitControlPoints(static_cast<int>(mesh.position.size()));
	FbxVector4* vertices = lMesh->GetControlPoints();
	
	std::transform(mesh.position.cbegin(), mesh.position.cend(),
				   vertices, [](const glm::vec3& vtx) 
		{ 
			return FbxVector4(vtx.x, vtx.y, vtx.z);
		});

	size_t triangleCount = mesh.indices.size() / 3;
	size_t vId           = 0;
	for (size_t i = 0; i != triangleCount; ++i)
	{
		lMesh->BeginPolygon();

		for (int v = 0; v < 3; v++)
		{
			lMesh->AddPolygon(mesh.indices[vId++]);
		}
			
		lMesh->EndPolygon();
	}

	// It seems that normals are dynamically generated
	// using compute shader or something else,
	// it's direction is not correct.
	
	assignNormal(mesh, lMesh);

	return lMesh;
}

std::vector<FbxNode*> FbxBuilder::createInstances(const MeshObject& info, FbxMesh* mesh)
{
	std::vector<FbxNode*> nodeInstances;
	uint32_t              instanceId = 0;

	auto toFbxDb3 = [](const glm::vec3& vec)
	{
		return FbxDouble3(vec[0], vec[1], vec[2]);
	};

	for (const auto& trs : info.instances)
	{
		// Create the node containing the mesh
		auto     nodeName = fmt::format("{}_{}", info.name, instanceId++);
		FbxNode* node     = FbxNode::Create(m_scene, nodeName.c_str());
		node->SetNodeAttribute(mesh);
		node->SetShadingMode(FbxNode::eTextureShading);

		// Apply transform
		node->LclTranslation.Set(toFbxDb3(trs.translation));
		node->LclRotation.Set(toFbxDb3(trs.rotation));
		node->LclScaling.Set(toFbxDb3(trs.scaling));

		nodeInstances.push_back(node);
	}

	return nodeInstances;
}

void FbxBuilder::assignNormal(const MeshObject& info, FbxMesh* mesh)
{
	//if (info.normal.empty())
	//{
	//	return;
	//}

	auto normals = ComputeNormalsWeightedByAngle(
		info.indices,
		info.position,
		true);

	FbxLayer* lLayer = mesh->GetLayer(0);
	if (lLayer == NULL)
	{
		mesh->CreateLayer();
		lLayer = mesh->GetLayer(0);
	}

	// Create a normal layer.
	FbxLayerElementNormal* lLayerElementNormal = FbxLayerElementNormal::Create(mesh, "");

	// Set its mapping mode to map each normal vector to each control point.
	lLayerElementNormal->SetMappingMode(FbxLayerElement::eByControlPoint);

	// Set the reference mode of so that the n'th element of the normal array maps to the n'th
	// element of the control point array.
	lLayerElementNormal->SetReferenceMode(FbxLayerElement::eDirect);

	// The normals in info.normal should be matched with info.position
	auto& lDirectArray = lLayerElementNormal->GetDirectArray();
	int   normalCount  = (int)normals.size();
	lDirectArray.Resize(normalCount);
	for (int i = 0; i != normalCount; ++i)
	{
		auto& n = normals[i];
		lDirectArray.SetAt(i, FbxVector4(n[0], n[1], n[2], 0.0f));
	}

	// Finally, we set layer 0 of the mesh to the normal layer element.
	lLayer->SetNormals(lLayerElementNormal);
}

void FbxBuilder::assignTexcoord(const MeshObject& info, FbxMesh* mesh)
{
	if (info.texcoord.empty())
	{
		return;
	}

	// Create UV for Diffuse channel
	FbxGeometryElementUV* lUVElement = mesh->CreateElementUV(m_uvName.c_str());
	FBX_ASSERT(lUVElement != NULL);
	lUVElement->SetMappingMode(FbxGeometryElement::eByControlPoint);
	lUVElement->SetReferenceMode(FbxGeometryElement::eDirect);

	auto& lDirectArray = lUVElement->GetDirectArray();
	int   coordCount   = (int)info.texcoord.size();
	lDirectArray.Resize(coordCount);
	for (int i = 0; i != coordCount; ++i)
	{
		auto& uv = info.texcoord[i];
		lDirectArray.SetAt(i, FbxVector2(uv[0], uv[1]));
	}

	auto nodeCount = mesh->GetNodeCount();
	for (int n = 0; n != nodeCount; ++n)
	{
		auto node = mesh->GetNode(n);
		node->SetShadingMode(FbxNode::eTextureShading);
	}
}

void FbxBuilder::createMaterial(const MeshObject& info, const std::vector<FbxNode*>& nodeList)
{
	for (auto& lNode : nodeList)
	{
		if (!lNode)
		{
			continue;
		}

		auto lMaterial = createMaterial(lNode);
		if (!lMaterial)
		{
			continue;
		}

		auto diffuse = findTexturePath(info, "_diffuse");
		if (!diffuse.empty())
		{
			auto lDiffuse = createTexture(diffuse);
			lMaterial->Diffuse.ConnectSrcObject(lDiffuse);
		}

		auto normal = findTexturePath(info, "_normal");
		if (!normal.empty())
		{
			auto lNormal = createTexture(normal);
			lMaterial->NormalMap.ConnectSrcObject(lNormal);
		}

		auto emissive = findTexturePath(info, "_emissive");
		if (!emissive.empty())
		{
			auto lEmissive = createTexture(emissive);
			lMaterial->Emissive.ConnectSrcObject(lEmissive);
		}

		lNode->AddMaterial(lMaterial);
	}
}

FbxSurfacePhong* FbxBuilder::createMaterial(FbxNode* lNode)
{
	FbxSurfacePhong* lMaterial = nullptr;
	do
	{
		FbxMesh* pMesh = lNode->GetMesh();
		if (!pMesh)
		{
			break;
		}
		// A texture need to be connected to a property on the material,
		// so let's use the material (if it exists) or create a new one
		lMaterial = lNode->GetSrcObject<FbxSurfacePhong>(0);

		if (lMaterial != NULL)
		{
			break;
		}

		FbxString  lMaterialName = "GowMaterial";
		FbxString  lShadingName  = "Phong";
		FbxDouble3 lBlack(0.0, 0.0, 0.0);
		FbxDouble3 lRed(1.0, 0.0, 0.0);
		FbxDouble3 lDiffuseColor(0.75, 0.75, 0.0);

		FbxLayer* lLayer = pMesh->GetLayer(0);

		// Create a layer element material to handle proper mapping.
		FbxLayerElementMaterial* lLayerElementMaterial = FbxLayerElementMaterial::Create(pMesh, lMaterialName.Buffer());

		// This allows us to control where the materials are mapped.  Using eAllSame
		// means that all faces/polygons of the mesh will be assigned the same material.
		lLayerElementMaterial->SetMappingMode(FbxLayerElement::eAllSame);
		lLayerElementMaterial->SetReferenceMode(FbxLayerElement::eIndexToDirect);

		// Save the material on the layer
		lLayer->SetMaterials(lLayerElementMaterial);

		// Add an index to the lLayerElementMaterial.  Since we have only one, and are using eAllSame mapping mode,
		// we only need to add one.
		lLayerElementMaterial->GetIndexArray().Add(0);

		lMaterial = FbxSurfacePhong::Create(m_scene, lMaterialName.Buffer());

		// Generate primary and secondary colors.
		//lMaterial->Emissive.Set(lBlack);
		//lMaterial->Ambient.Set(lRed);
		lMaterial->AmbientFactor.Set(1.);
		// Add texture for diffuse channel
		//lMaterial->Diffuse.Set(lDiffuseColor);
		lMaterial->DiffuseFactor.Set(1.);
		lMaterial->TransparencyFactor.Set(0.4);
		lMaterial->ShadingModel.Set(lShadingName);
		//lMaterial->Shininess.Set(0.5);
		//lMaterial->Specular.Set(lBlack);
		lMaterial->SpecularFactor.Set(0.3);
	} while (false);
	return lMaterial;
}

FbxFileTexture* FbxBuilder::createTexture(const std::string& filename)
{
	auto            basename = fs::path(filename).filename().string();
	FbxFileTexture* lTexture = FbxFileTexture::Create(m_scene, basename.c_str());
	// Set texture properties.
	lTexture->SetFileName(filename.c_str());  // Resource file is in current directory.
	lTexture->SetTextureUse(FbxTexture::eStandard);
	lTexture->SetMappingType(FbxTexture::eUV);
	lTexture->SetMaterialUse(FbxFileTexture::eModelMaterial);
	lTexture->SetSwapUV(false);
	lTexture->SetTranslation(0.0, 0.0);
	lTexture->SetScale(1.0, 1.0);
	lTexture->SetRotation(0.0, 0.0);
	lTexture->UVSet.Set(FbxString(m_uvName.c_str()));  // Connect texture to the proper UV

	return lTexture;
}

std::string FbxBuilder::findTexturePath(const MeshObject& info, const std::string& name)
{
	for (const auto& resName : info.textures)
	{
		if (resName.find(name) != std::string::npos)
		{
			return resName;
		}
	}
	return std::string();
}

glm::vec3 MultiplyAdd(const glm::vec3& V1, const glm::vec3& V2, const glm::vec3& V3)
{
	return glm::vec3(V1.x * V2.x + V3.x,
					 V1.y * V2.y + V3.y,
					 V1.z * V2.z + V3.z);
}

std::vector<glm::vec3> FbxBuilder::ComputeNormalsWeightedByAngle(
	const std::vector<uint32_t>&  indices,
	const std::vector<glm::vec3>& positions,
	bool                          cw)
{
	size_t nFaces = indices.size() / 3;
	size_t nVerts = positions.size();

	std::vector<glm::vec3> vertNormals(nVerts, glm::vec3(0.0));

	for (size_t face = 0; face < nFaces; ++face)
	{
		uint32_t i0 = indices[face * 3];
		uint32_t i1 = indices[face * 3 + 1];
		uint32_t i2 = indices[face * 3 + 2];

		if (i0 == -1 || i1 == -1 || i2 == -1)
		{
			continue;
		}

		if (i0 >= nVerts || i1 >= nVerts || i2 >= nVerts)
		{
			return std::vector<glm::vec3>();
		}

		const glm::vec3 p0 = positions[i0];
		const glm::vec3 p1 = positions[i1];
		const glm::vec3 p2 = positions[i2];

		const glm::vec3 u = p1 - p0;
		const glm::vec3 v = p2 - p0;

		const glm::vec3 faceNormal = glm::normalize(glm::cross(u, v));

		// Corner 0 -> 1 - 0, 2 - 0
		const glm::vec3 a   = glm::normalize(u);
		const glm::vec3 b   = glm::normalize(v);
		float           dot = glm::dot(a, b);
		dot                 = glm::clamp(dot, -1.0f, 1.0f);
		float     angle     = glm::acos(dot);
		glm::vec3 w0(angle, angle, angle);

		// Corner 1 -> 2 - 1, 0 - 1
		const glm::vec3 c = glm::normalize(p2 - p1);
		const glm::vec3 d = glm::normalize(p0 - p1);
		dot               = glm::dot(c, d);
		dot               = glm::clamp(dot, -1.0f, 1.0f);
		angle             = glm::acos(dot);
		glm::vec3 w1(angle, angle, angle);

		// Corner 2 -> 0 - 2, 1 - 2
		const glm::vec3 e = glm::normalize(p0 - p2);
		const glm::vec3 f = glm::normalize(p1 - p2);
		dot               = glm::dot(e, f);
		dot               = glm::clamp(dot, -1.0f, 1.0f);
		angle             = glm::acos(dot);
		glm::vec3 w2(angle, angle, angle);

		vertNormals[i0] = MultiplyAdd(faceNormal, w0, vertNormals[i0]);
		vertNormals[i1] = MultiplyAdd(faceNormal, w1, vertNormals[i1]);
		vertNormals[i2] = MultiplyAdd(faceNormal, w2, vertNormals[i2]);
	}

	// Store results
	std::vector<glm::vec3> normals(nVerts);
	if (cw)
	{
		for (size_t vert = 0; vert < nVerts; ++vert)
		{
			glm::vec3 n = glm::normalize(vertNormals[vert]);
			normals[vert] = -n;
		}
	}
	else
	{
		for (size_t vert = 0; vert < nVerts; ++vert)
		{
			const glm::vec3 n = glm::normalize(vertNormals[vert]);
			normals[vert]     = n;
		}
	}

	return normals;
}

void FbxBuilder::initializeSdkObjects()
{
	// The first thing to do is to create the FBX Manager 
	// which is the object allocator for almost all the classes in the SDK
	m_manager = FbxManager::Create();

	LOG_ASSERT(m_manager != nullptr, "Unable to create FBX Manager!\n");
	LOG_DEBUG("Autodesk FBX SDK version %s\n", m_manager->GetVersion());

	// Create an IOSettings object. This object holds all import/export settings.
	FbxIOSettings* ios = FbxIOSettings::Create(m_manager, IOSROOT);
	m_manager->SetIOSettings(ios);

	// Load plugins from the executable directory (optional)
	FbxString lPath = FbxGetApplicationDirectory();
	m_manager->LoadPluginsDirectory(lPath.Buffer());

	// Create an FBX scene. This object holds most objects imported/exported from/to files.
	m_scene = FbxScene::Create(m_manager, "GowScene");
	LOG_ASSERT(m_scene != nullptr, "Unable to create FBX scene!\n");
}

void FbxBuilder::destroySdkObjects()
{
	// Delete the FBX Manager. 
	// All the objects that have been allocated using the FBX Manager 
	// and that haven't been explicitly destroyed are also automatically destroyed.
	if (m_manager)
	{
		m_manager->Destroy();
		m_manager = nullptr;
	}
}


