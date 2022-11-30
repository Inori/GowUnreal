#include "FBXSerializer.h"
#include "gtc/matrix_access.hpp"
#include "gtc/constants.hpp"
#include "gtx/euler_angles.hpp"

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
            const auto& mesh = expMeshes[i];

            FBXSDK_printf("Process mesh %s\n\n", mesh.name.c_str());

            auto node = createMesh(mesh);

            if (armature.boneCount > 0)
            {
                bindSkeleton(expMeshes[i], armature, node);
            }

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

void FbxSdkManager::bindSkeleton(const RawMeshContainer& rawMesh, const Rig& armature, FbxNode* meshNode)
{
    FbxMesh* mesh = meshNode->GetMesh();

    FbxSkin* lSkin = FbxSkin::Create(m_scene, "");
   
    auto skeletons = createSkeleton(armature);
    
    FbxNode* rootSkeleton = skeletons[0];

    linkSkeleton(rawMesh, armature, lSkin, rootSkeleton);

    mesh->AddDeformer(lSkin);
}

std::vector<FbxNode*> FbxSdkManager::createSkeleton(const Rig& armature)
{
    auto toFbxDb3 = [](const glm::vec3& vec)
    {
        return FbxDouble3(vec[0], vec[1], vec[2]);
    };

    std::vector<FbxNode*> nodes;
    nodes.reserve(armature.boneCount);

    for (uint16_t i = 0; i < armature.boneCount; ++i)
    {
        std::string name = armature.boneNames[i] + "_" + std::string("J") + std::to_string(i);

        glm::mat4 boneMatrix;
        std::memcpy(&boneMatrix, &armature.matrix[i], sizeof(boneMatrix));
        auto trs = decomposeTransform(boneMatrix);

        FbxSkeleton* lSkeletonAttribute = FbxSkeleton::Create(m_scene, name.c_str());
        FbxNode* node = FbxNode::Create(m_scene, name.c_str());
        node->SetNodeAttribute(lSkeletonAttribute);
        node->SetUserDataPtr(0, reinterpret_cast<void*>(i)); // record bone id
        // Apply transform
        node->LclTranslation.Set(toFbxDb3(trs.translation));
        node->LclRotation.Set(toFbxDb3(trs.rotation));
        node->LclScaling.Set(toFbxDb3(trs.scaling));

        int16_t parentId = armature.boneParents[i];
        if (parentId > -1)
        {
            // the skeleton has parent, so it's eLimbNode
            lSkeletonAttribute->SetSkeletonType(FbxSkeleton::eLimbNode);
            lSkeletonAttribute->Size.Set(1.0);

            auto& parentNode = nodes[parentId];
            parentNode->AddChild(node);
        }
        else
        {
            // the skeleton doesn't have parent, so it's eRoot
            lSkeletonAttribute->SetSkeletonType(FbxSkeleton::eRoot);
        }

        nodes.push_back(node);
    }

    return nodes;
}

void FbxSdkManager::linkSkeleton(const RawMeshContainer& rawMesh, const Rig& armature, FbxSkin* skin, FbxNode* skeleton)
{
    auto toFbxMatrix = [](Matrix4x4& in)
    {
        FbxAMatrix out;
        for (size_t r = 0; r < 4; r++)
        {
            for (size_t c = 0; c < 4; c++)
            {
                out[r][c] = in[r][c];
            }
        }
        return out;
    };

    FbxCluster* lCluster = FbxCluster::Create(m_scene, "");
    lCluster->SetLink(skeleton);
    lCluster->SetLinkMode(FbxCluster::eTotalOne);

    uint16_t boneId = reinterpret_cast<uint16_t>(skeleton->GetUserDataPtr(0));
    for (size_t vId = 0; vId != rawMesh.VertCount; ++vId)
    {
        uint16_t* joints = rawMesh.joints[vId];
        float* weights = rawMesh.weights[vId];
        for (size_t j = 0; j != 4; ++j)
        {
            uint16_t bindId = joints[j];
            if (bindId == boneId)
            {
                lCluster->AddControlPointIndex(vId, weights[j]);
                break;
            }
        }
    }

    FbxAMatrix boneMatrix = skeleton->EvaluateGlobalTransform();
    FbxAMatrix offsetMatrix = toFbxMatrix(armature.IBMs[boneId]);
    FbxAMatrix meshMatrix = offsetMatrix * boneMatrix;

    lCluster->SetTransformMatrix(meshMatrix);
    lCluster->SetTransformLinkMatrix(boneMatrix);

    skin->AddCluster(lCluster);

    int childCount = skeleton->GetChildCount();
    for (int k = 0; k != childCount; ++k)
    {
        FbxNode* child = skeleton->GetChild(k);
        linkSkeleton(rawMesh, armature, skin, child);
    }
}

FbxSdkManager::MeshTransform FbxSdkManager::decomposeTransform(const glm::mat4& transform)
{
    MeshTransform result = {};

    result.translation.x = transform[3][0];
    result.translation.y = transform[3][1];
    result.translation.z = transform[3][2];

    result.scaling.x = glm::length(glm::column(transform, 0));
    result.scaling.y = glm::length(glm::column(transform, 1));
    result.scaling.z = glm::length(glm::column(transform, 2));

    const auto& scaling = result.scaling;
    // get upper 3x3 matrix
    glm::mat3 upper = glm::mat3(transform);
    glm::column(upper, 0, glm::column(upper, 0) / scaling.x);
    glm::column(upper, 1, glm::column(upper, 1) / scaling.y);
    glm::column(upper, 2, glm::column(upper, 2) / scaling.z);

    // default rotation order in FBX SDK is of XYZ, R = Rx * Ry * Rz
    // so we need to rotate across Z then Y then X
    glm::mat4 rotation = glm::mat4(upper);
    glm::vec3 radians = {};
    glm::extractEulerAngleZYX(rotation, radians.z, radians.y, radians.x);

    result.rotation = glm::degrees(radians);

    return result;

    return result;
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

