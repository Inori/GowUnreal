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

bool MeshObject::operator==(const MeshObject& other)
{
	return position == other.position &&
		   transform == other.transform;
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
	auto meshNode = createMesh(mesh);

	// Add node to node tree.
	FbxNode* rootNode = m_scene->GetRootNode();
	rootNode->AddChild(meshNode);
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
				const char* lASCII = "ascii";
				if (lDesc.Find(lASCII) >= 0)
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

FbxNode* FbxBuilder::createMesh(const MeshObject& mesh)
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

	// Create the node containing the mesh
	FbxNode* node = FbxNode::Create(m_scene, mesh.name.c_str());
	node->SetNodeAttribute(lMesh);
	node->SetShadingMode(FbxNode::eTextureShading);

	return node;
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


