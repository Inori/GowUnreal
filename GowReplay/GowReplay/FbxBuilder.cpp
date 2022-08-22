#include "FbxBuilder.h"
#include "Log.h"

#include <algorithm>

#ifdef IOS_REF
#undef IOS_REF
#define IOS_REF (*(m_manager->GetIOSettings()))
#endif


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
	createMaterial(meshNodes);

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
	if (info.normal.empty())
	{
		return;
	}

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
	int   normalCount  = (int)info.normal.size();
	lDirectArray.Resize(normalCount);
	for (int i = 0; i != normalCount; ++i)
	{
		auto& n = info.normal[i];
		lDirectArray.SetAt(i, FbxVector4(n[0], n[1], n[2], n[3]));
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

void FbxBuilder::createMaterial(const std::vector<FbxNode*>& nodeList)
{
	for (auto& lNode : nodeList)
	{
		if (!lNode)
		{
			continue;
		}

		FbxMesh* pMesh = lNode->GetMesh();

		// A texture need to be connected to a property on the material,
		// so let's use the material (if it exists) or create a new one
		FbxSurfacePhong* lMaterial = lNode->GetSrcObject<FbxSurfacePhong>(0);

		if (lMaterial != NULL)
		{
			continue;
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
		lMaterial->Emissive.Set(lBlack);
		lMaterial->Ambient.Set(lRed);
		lMaterial->AmbientFactor.Set(1.);
		// Add texture for diffuse channel
		lMaterial->Diffuse.Set(lDiffuseColor);
		lMaterial->DiffuseFactor.Set(1.);
		lMaterial->TransparencyFactor.Set(0.4);
		lMaterial->ShadingModel.Set(lShadingName);
		lMaterial->Shininess.Set(0.5);
		lMaterial->Specular.Set(lBlack);
		lMaterial->SpecularFactor.Set(0.3);

		lNode->AddMaterial(lMaterial);
	}
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


