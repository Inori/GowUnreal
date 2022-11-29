#include "FBXSerializer.h"

#ifdef IOS_REF
#undef  IOS_REF
#define IOS_REF (*(m_manager->GetIOSettings()))
#endif

FbxSdkManager::FbxSdkManager()
{
    initializeSdkObjects();
}

FbxSdkManager::~FbxSdkManager()
{
    destroySdkObjects();
}

void FbxSdkManager::writeFbx(
    const std::filesystem::path& path,
    const vector<RawMeshContainer>& expMeshes,
    const Rig& armature)
{
    do 
    {
        m_scene = FbxScene::Create(m_manager, "GowScene");
        if (!m_scene)
        {
            break;
        }

        FbxNode* rootNode = m_scene->GetRootNode();

        for (int i = 0; i < expMeshes.size(); i++)
        {
            auto node = createMesh(expMeshes[i]);
            rootNode->AddChild(node);
        }

        saveScene(path.string().c_str());
        
    } while (false);
}

FbxNode* FbxSdkManager::createMesh(const RawMeshContainer& rawMesh)
{
    FbxMesh* lMesh = FbxMesh::Create(m_scene, "");

    // Create control points.
    lMesh->InitControlPoints(rawMesh.VertCount);
    FbxVector4* vertices = lMesh->GetControlPoints();

    std::transform(rawMesh.vertices, rawMesh.vertices + rawMesh.VertCount,
        vertices, [](const Vec3& vtx)
        {
            return FbxVector4(vtx.X, vtx.Y, vtx.Z);
        });

    size_t triangleCount = rawMesh.IndCount / 3;
    size_t vId = 0;
    for (size_t i = 0; i != triangleCount; ++i)
    {
        lMesh->BeginPolygon();

        for (int v = 0; v < 3; v++)
        {
            lMesh->AddPolygon(rawMesh.indices[vId++]);
        }

        lMesh->EndPolygon();
    }

    assignNormal(rawMesh, lMesh);
    assignTangent(rawMesh, lMesh);
    assignTexcoord(rawMesh, lMesh);

    FbxNode* node = FbxNode::Create(m_scene, rawMesh.name.c_str());
    node->SetNodeAttribute(lMesh);
    node->SetShadingMode(FbxNode::eTextureShading);
    return node;
}

void FbxSdkManager::assignNormal(const RawMeshContainer& rawMesh, FbxMesh* mesh)
{
    if (!rawMesh.normals)
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
    int   normalCount = (int)rawMesh.VertCount;
    lDirectArray.Resize(normalCount);
    for (int i = 0; i != normalCount; ++i)
    {
        auto& n = rawMesh.normals[i];
        lDirectArray.SetAt(i, FbxVector4(n.X, n.Y, n.Z, 0.0f));
    }

    // Finally, we set layer 0 of the mesh to the normal layer element.
    lLayer->SetNormals(lLayerElementNormal);
}

void FbxSdkManager::assignTexcoord(const RawMeshContainer& rawMesh, FbxMesh* mesh)
{
    assignTexcoord(rawMesh.txcoord0, rawMesh.VertCount, mesh);
    assignTexcoord(rawMesh.txcoord1, rawMesh.VertCount, mesh);
    assignTexcoord(rawMesh.txcoord2, rawMesh.VertCount, mesh);
}

void FbxSdkManager::assignTexcoord(Vec2* texcoord, uint32_t count, FbxMesh* mesh)
{
    if (!texcoord || !count)
    {
        return;
    }

    static uint32_t uvLayerId = 0;
    std::string uvName = std::string("UV") + std::to_string(uvLayerId++);

    // Create UV for Diffuse channel
    FbxGeometryElementUV* lUVElement = mesh->CreateElementUV(uvName.c_str());
    FBX_ASSERT(lUVElement != NULL);
    lUVElement->SetMappingMode(FbxGeometryElement::eByControlPoint);
    lUVElement->SetReferenceMode(FbxGeometryElement::eDirect);

    auto& lDirectArray = lUVElement->GetDirectArray();
    int   coordCount = (int)count;
    lDirectArray.Resize(coordCount);
    for (int i = 0; i != coordCount; ++i)
    {
        auto& uv = texcoord[i];
        lDirectArray.SetAt(i, FbxVector2(uv.X, uv.Y));
    }

    auto nodeCount = mesh->GetNodeCount();
    for (int n = 0; n != nodeCount; ++n)
    {
        auto node = mesh->GetNode(n);
        node->SetShadingMode(FbxNode::eTextureShading);
    }
}

void FbxSdkManager::assignTangent(const RawMeshContainer& rawMesh, FbxMesh* mesh)
{
    if (!rawMesh.tangents)
    {
        return;
    }

    FbxLayer* lLayer = mesh->GetLayer(0);
    if (lLayer == NULL)
    {
        mesh->CreateLayer();
        lLayer = mesh->GetLayer(0);
    }

    // Create a tangent layer.
    FbxLayerElementTangent* lLayerElementTangent = FbxLayerElementTangent::Create(mesh, "");

    // Set its mapping mode to map each normal vector to each control point.
    lLayerElementTangent->SetMappingMode(FbxLayerElement::eByControlPoint);

    // Set the reference mode of so that the n'th element of the normal array maps to the n'th
    // element of the control point array.
    lLayerElementTangent->SetReferenceMode(FbxLayerElement::eDirect);

    // The normals in info.normal should be matched with info.position
    auto& lDirectArray = lLayerElementTangent->GetDirectArray();
    int   tangetCount = (int)rawMesh.VertCount;
    lDirectArray.Resize(tangetCount);
    for (int i = 0; i != tangetCount; ++i)
    {
        auto& t = rawMesh.tangents[i];
        lDirectArray.SetAt(i, FbxVector4(t.X, t.Y, t.Z, 0.0f));
    }

    // Finally, we set layer 0 of the mesh to the normal layer element.
    lLayer->SetTangents(lLayerElementTangent);
}

void FbxSdkManager::initializeSdkObjects()
{
    //The first thing to do is to create the FBX Manager which is the object allocator for almost all the classes in the SDK
    m_manager = FbxManager::Create();
    if (!m_manager)
    {
        FBXSDK_printf("Error: Unable to create FBX Manager!\n");
        exit(1);
    }
    else FBXSDK_printf("Autodesk FBX SDK version %s\n", m_manager->GetVersion());

    //Create an IOSettings object. This object holds all import/export settings.
    FbxIOSettings* ios = FbxIOSettings::Create(m_manager, IOSROOT);
    m_manager->SetIOSettings(ios);

    //Load plugins from the executable directory (optional)
    FbxString lPath = FbxGetApplicationDirectory();
    m_manager->LoadPluginsDirectory(lPath.Buffer());
}

void FbxSdkManager::destroySdkObjects()
{
    if (m_manager)
    {
        m_manager->Destroy();
        m_manager = nullptr;
    }
}

bool FbxSdkManager::saveScene(const char* pFilename, int pFileFormat /*= -1*/, bool pEmbedMedia /*= false*/)
{
    int lMajor, lMinor, lRevision;
    bool lStatus = true;

    // Create an exporter.
    FbxExporter* lExporter = FbxExporter::Create(m_manager, "");

    if (pFileFormat < 0 || pFileFormat >= m_manager->GetIOPluginRegistry()->GetWriterFormatCount())
    {
        // Write in fall back format in less no ASCII format found
        pFileFormat = m_manager->GetIOPluginRegistry()->GetNativeWriterFormat();

        //Try to export in ASCII if possible
        int lFormatIndex, lFormatCount = m_manager->GetIOPluginRegistry()->GetWriterFormatCount();

        for (lFormatIndex = 0; lFormatIndex < lFormatCount; lFormatIndex++)
        {
            if (m_manager->GetIOPluginRegistry()->WriterIsFBX(lFormatIndex))
            {
                FbxString lDesc = m_manager->GetIOPluginRegistry()->GetWriterFormatDescription(lFormatIndex);
                const char* lBinary = "binary";
                if (lDesc.Find(lBinary) >= 0)
                {
                    pFileFormat = lFormatIndex;
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
    if (lExporter->Initialize(pFilename, pFileFormat, m_manager->GetIOSettings()) == false)
    {
        FBXSDK_printf("Call to FbxExporter::Initialize() failed.\n");
        FBXSDK_printf("Error returned: %s\n\n", lExporter->GetStatus().GetErrorString());
        return false;
    }

    FbxManager::GetFileFormatVersion(lMajor, lMinor, lRevision);
    FBXSDK_printf("FBX file format version %d.%d.%d\n\n", lMajor, lMinor, lRevision);

    // Export the scene.
    lStatus = lExporter->Export(m_scene);

    // Destroy the exporter.
    lExporter->Destroy();
    return lStatus;
}

//////////////////////////////////////////////////////////////////////////

void writeFbx(const std::filesystem::path& path, const vector<RawMeshContainer>& expMeshes, const Rig& armature)
{
    FbxSdkManager bbxManager;
    bbxManager.writeFbx(path, expMeshes, armature);
}

