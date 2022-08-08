#include "glTFSerializer.h"
#include "pch.h"

#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/BufferBuilder.h>
#include <GLTFSDK/GLTFResourceWriter.h>
#include <GLTFSDK/GLBResourceWriter.h>
#include <GLTFSDK/IStreamWriter.h>
#include <GLTFSDK/Serialize.h>

#include <sstream>
#include <cassert>
#include <cstdlib>

using namespace Microsoft::glTF;

// The glTF SDK is decoupled from all file I/O by the IStreamWriter (and IStreamReader)
// interface(s) and the C++ stream-based I/O library. This allows the glTF SDK to be used in
// sandboxed environments, such as WebAssembly modules and UWP apps, where any file I/O code
// must be platform or use-case specific.
class StreamWriter : public IStreamWriter
{
public:
    StreamWriter(std::filesystem::path pathBase) : m_pathBase(std::move(pathBase))
    {
        assert(m_pathBase.has_root_path());
    }

    ~StreamWriter() 
    {
        for (auto& stream : m_streams)
        {
            if (stream->is_open())
            {
                stream->close();
            }
        }
        m_streams.clear();
    }
    // Resolves the relative URIs of any external resources declared in the glTF manifest
    std::shared_ptr<std::ostream> GetOutputStream(const std::string& filename) const override
    {
        // In order to construct a valid stream:
        // 1. The filename argument will be encoded as UTF-8 so use filesystem::u8path to
        //    correctly construct a path instance.
        // 2. Generate an absolute path by concatenating m_pathBase with the specified filename
        //    path. The filesystem::operator/ uses the platform's preferred directory separator
        //    if appropriate.
        // 3. Always open the file stream in binary mode. The glTF SDK will handle any text
        //    encoding issues for us.
        auto streamPath = m_pathBase / std::filesystem::path(filename);
        auto stream = std::make_shared<std::ofstream>(streamPath, std::ios_base::binary);

        // Check if the stream has no errors and is ready for I/O operations
        if (!stream || !(*stream))
        {
            cout << "open stream failed for reason " << errno << '\n';
            throw std::runtime_error("Unable to create a valid output stream for uri: " + filename);
        }

        m_streams.push_back(stream);

        return stream;
    }

private:
    std::filesystem::path m_pathBase;
    mutable std::vector<std::shared_ptr<std::ofstream>> m_streams;
};
auto AddMesh(Document& document, BufferBuilder& bufferBuilder, const RawMeshContainer& expMesh, bool Skinned)
{
    MeshPrimitive meshPrimitive;
    meshPrimitive.materialId = document.materials.Front().id;

    // Create a BufferView with a target of ELEMENT_ARRAY_BUFFER (as it will reference index
    // data) - it will be the 'current' BufferView that all the Accessors created by this
    // BufferBuilder will automatically reference
    if (expMesh.indices != nullptr)
    {
        bufferBuilder.AddBufferView(BufferViewTarget::ELEMENT_ARRAY_BUFFER);

        // Add an Accessor for the indices
        std::vector<uint16_t> indices;

        for (uint32_t i = 0; i < expMesh.IndCount; i += 3)
        {
            indices.push_back(expMesh.indices[i + 1]);
            indices.push_back(expMesh.indices[i]);
            indices.push_back(expMesh.indices[i + 2]);
        }
        // Copy the Accessor's id - subsequent calls to AddAccessor may invalidate the returned reference
        meshPrimitive.indicesAccessorId = bufferBuilder.AddAccessor(indices, { TYPE_SCALAR, COMPONENT_UNSIGNED_SHORT }).id;
    }
    
    if (expMesh.vertices != nullptr)
    {
        // Create a BufferView with target ARRAY_BUFFER (as it will reference vertex attribute data)
        bufferBuilder.AddBufferView(BufferViewTarget::ARRAY_BUFFER);
        // Add an Accessor for the positions
        std::vector<float> positions;
        for (uint32_t i = 0; i < expMesh.VertCount; i++)
        {
            positions.push_back(expMesh.vertices[i].X);
            positions.push_back(expMesh.vertices[i].Y);
            positions.push_back(expMesh.vertices[i].Z);
        }

        std::vector<float> minValues(3U, std::numeric_limits<float>::max());
        std::vector<float> maxValues(3U, std::numeric_limits<float>::lowest());

        const size_t positionCount = positions.size();

        // Accessor min/max properties must be set for vertex position data so calculate them here
        for (size_t i = 0U, j = 0U; i < positionCount; ++i, j = (i % 3U))
        {
            minValues[j] = std::min(positions[i], minValues[j]);
            maxValues[j] = std::max(positions[i], maxValues[j]);
        }

        meshPrimitive.attributes[ACCESSOR_POSITION] = bufferBuilder.AddAccessor(positions, { TYPE_VEC3, COMPONENT_FLOAT, false, std::move(minValues), std::move(maxValues) }).id;
    }

    if (expMesh.normals != nullptr)
    {
        // Create a BufferView with target ARRAY_BUFFER (as it will reference vertex attribute data)
        bufferBuilder.AddBufferView(BufferViewTarget::ARRAY_BUFFER);
        // Add an Accessor for the normals
        std::vector<float> normals;
        for (uint32_t i = 0; i < expMesh.VertCount; i++)
        {
            normals.push_back(expMesh.normals[i].X);
            normals.push_back(expMesh.normals[i].Y);
            normals.push_back(expMesh.normals[i].Z);
        }
        meshPrimitive.attributes[ACCESSOR_NORMAL] = bufferBuilder.AddAccessor(normals, { TYPE_VEC3, COMPONENT_FLOAT }).id;
    }

    if (expMesh.tangents != nullptr)
    {
        // Create a BufferView with target ARRAY_BUFFER (as it will reference vertex attribute data)
        bufferBuilder.AddBufferView(BufferViewTarget::ARRAY_BUFFER);
        // Add an Accessor for the tangents
        std::vector<float> tangents;
        for (uint32_t i = 0; i < expMesh.VertCount; i++)
        {
            tangents.push_back(expMesh.tangents[i].X);
            tangents.push_back(expMesh.tangents[i].Y);
            tangents.push_back(expMesh.tangents[i].Z);
            tangents.push_back(expMesh.tangents[i].W);
        }
        meshPrimitive.attributes[ACCESSOR_TANGENT] = bufferBuilder.AddAccessor(tangents, { TYPE_VEC4, COMPONENT_FLOAT }).id;
    }


    if (expMesh.txcoord0 != nullptr)
    {
        // Create a BufferView with target ARRAY_BUFFER (as it will reference vertex attribute data)
        bufferBuilder.AddBufferView(BufferViewTarget::ARRAY_BUFFER);
        std::vector<float> texcoords0;
        for (uint32_t i = 0; i < expMesh.VertCount; i++)
        {
            texcoords0.push_back(expMesh.txcoord0[i].X);
            texcoords0.push_back(expMesh.txcoord0[i].Y);
        }
        meshPrimitive.attributes[ACCESSOR_TEXCOORD_0] = bufferBuilder.AddAccessor(texcoords0, { TYPE_VEC2, COMPONENT_FLOAT }).id;
    }

    if (expMesh.txcoord1 != nullptr)
    {
        // Create a BufferView with target ARRAY_BUFFER (as it will reference vertex attribute data)
        bufferBuilder.AddBufferView(BufferViewTarget::ARRAY_BUFFER);
        std::vector<float> texcoords1;
        for (uint32_t i = 0; i < expMesh.VertCount; i++)
        {
            texcoords1.push_back(expMesh.txcoord1[i].X);
            texcoords1.push_back(expMesh.txcoord1[i].Y);
        }
        meshPrimitive.attributes[ACCESSOR_TEXCOORD_1] = bufferBuilder.AddAccessor(texcoords1, { TYPE_VEC2, COMPONENT_FLOAT }).id;
    }

    if (expMesh.txcoord2 != nullptr)
    {
        // Create a BufferView with target ARRAY_BUFFER (as it will reference vertex attribute data)
        bufferBuilder.AddBufferView(BufferViewTarget::ARRAY_BUFFER);
        std::vector<float> texcoords2;
        for (uint32_t i = 0; i < expMesh.VertCount; i++)
        {
            texcoords2.push_back(expMesh.txcoord2[i].X);
            texcoords2.push_back(expMesh.txcoord2[i].Y);
        }
        meshPrimitive.attributes["TEXCOORD_2"] = bufferBuilder.AddAccessor(texcoords2, { TYPE_VEC2, COMPONENT_FLOAT }).id;
    }

    if (expMesh.joints != nullptr)
    {
        // Create a BufferView with target ARRAY_BUFFER (as it will reference vertex attribute data)
        bufferBuilder.AddBufferView(BufferViewTarget::ARRAY_BUFFER);
        std::vector<uint16_t> joints0;
        for (uint32_t i = 0; i < expMesh.VertCount; i++)
        {
            joints0.push_back(expMesh.joints[i][0]);
            joints0.push_back(expMesh.joints[i][1]);
            joints0.push_back(expMesh.joints[i][2]);
            joints0.push_back(expMesh.joints[i][3]);
        }
        meshPrimitive.attributes[ACCESSOR_JOINTS_0] = bufferBuilder.AddAccessor(joints0, { TYPE_VEC4, COMPONENT_UNSIGNED_SHORT }).id;
    }
    if (expMesh.weights != nullptr)
    {
        // Create a BufferView with target ARRAY_BUFFER (as it will reference vertex attribute data)
        bufferBuilder.AddBufferView(BufferViewTarget::ARRAY_BUFFER);
        std::vector<float> weights0;
        for (uint32_t i = 0; i < expMesh.VertCount; i++)
        {
            weights0.push_back(expMesh.weights[i][0]);
            weights0.push_back(expMesh.weights[i][1]);
            weights0.push_back(expMesh.weights[i][2]);
            weights0.push_back(expMesh.weights[i][3]);
        }
        meshPrimitive.attributes[ACCESSOR_WEIGHTS_0] = bufferBuilder.AddAccessor(weights0, { TYPE_VEC4, COMPONENT_FLOAT }).id;
    }

    // Create a very simple glTF Document with the following hierarchy:
    //  Scene
    //     Node
    //       Mesh (Triangle)
    //         MeshPrimitive
    //           Material (Blue)
    // 
    // A Document can be constructed top-down or bottom up. However, if constructed top-down
    // then the IDs of child entities must be known in advance, which prevents using the glTF
    // SDK's automatic ID generation functionality.

    // Construct a Material
    /*
    Material material;
    material.metallicRoughness.baseColorFactor = Color4(0.0f, 0.0f, 1.0f, 1.0f);
    material.metallicRoughness.metallicFactor = 0.2f;
    material.metallicRoughness.roughnessFactor = 0.4f;
    material.doubleSided = true;

    // Add it to the Document and store the generated ID
    //auto materialId = document.materials.Append(std::move(material), AppendIdPolicy::GenerateOnEmpty).id;
    */

    // Construct a MeshPrimitive. Unlike most types in glTF, MeshPrimitives are direct children
    // of their parent Mesh entity rather than being children of the Document. This is why they
    // don't have an ID member.

    // Construct a Mesh and add the MeshPrimitive as a child
    Mesh mesh;
    mesh.name = expMesh.name;
    mesh.primitives.push_back(std::move(meshPrimitive));
    // Add it to the Document and store the generated ID
    auto meshId = document.meshes.Append(std::move(mesh), AppendIdPolicy::GenerateOnEmpty).id;

    // Construct a Node adding a reference to the Mesh
    Node node;
    node.meshId = meshId;
    node.name = expMesh.name;
    if(Skinned)
    node.skinId = document.skins.Front().id;
    // Add it to the Document and store the generated ID
    auto nodeId = document.nodes.Append(std::move(node), AppendIdPolicy::GenerateOnEmpty).id;

    return nodeId;
}
void WriteGLTF(const std::filesystem::path& path, const vector<RawMeshContainer>& expMeshes, const Rig& Armature)
{
    // Pass the absolute path, without the filename, to the stream writer
    auto streamWriter = std::make_unique<StreamWriter>(path.parent_path());

    std::filesystem::path pathFile = path.filename();
    std::filesystem::path pathFileExt = pathFile.extension();

    auto MakePathExt = [](const std::string& ext)
    {
        return "." + ext;
    };

    std::unique_ptr<ResourceWriter> resourceWriter;

    // If the file has a '.gltf' extension then create a GLTFResourceWriter
    if (pathFileExt == MakePathExt(GLTF_EXTENSION))
    {
        resourceWriter = std::make_unique<GLTFResourceWriter>(std::move(streamWriter));
    }

    // If the file has a '.glb' extension then create a GLBResourceWriter. This class derives
    // from GLTFResourceWriter and adds support for writing manifests to a GLB container's
    // JSON chunk and resource data to the binary chunk.
    if (pathFileExt == MakePathExt(GLB_EXTENSION))
    {
        resourceWriter = std::make_unique<GLBResourceWriter>(std::move(streamWriter));
    }

    if (!resourceWriter)
    {
        throw std::runtime_error("Command line argument path filename extension must be .gltf or .glb");
    }

    // The Document instance represents the glTF JSON manifest
    Document document;
    document.asset.copyright = "Santa Monica Studios";
    document.asset.generator = "God of War Tool - HitmanHimself";
    // Use the BufferBuilder helper class to simplify the process of
    // constructing valid glTF Buffer, BufferView and Accessor entities
    BufferBuilder bufferBuilder(std::move(resourceWriter));

    // Create all the resource data (e.g. triangle indices and
    // vertex positions) that will be written to the binary buffer
    const char* bufferId = nullptr;

    // Specify the 'special' GLB buffer ID. This informs the GLBResourceWriter that it should use
    // the GLB container's binary chunk (usually the desired buffer location when creating GLBs)
    if (dynamic_cast<const GLBResourceWriter*>(&bufferBuilder.GetResourceWriter()))
    {
        bufferId = GLB_BUFFER_ID;
    }

    // Create a Buffer - it will be the 'current' Buffer that all the BufferViews
    // created by this BufferBuilder will automatically reference
    bufferBuilder.AddBuffer(bufferId);

    Material material;
    material.name = "Default";
    material.metallicRoughness.baseColorFactor = Color4(1.0f, 1.0f, 1.0f, 1.0f);
    material.metallicRoughness.metallicFactor = 0.0f;
    material.metallicRoughness.roughnessFactor = 0.5f;
    material.doubleSided = true;
    document.materials.Append(std::move(material), AppendIdPolicy::GenerateOnEmpty);

    // Construct a Scene
    Scene scene;
    scene.name = "Scene";
    if (Armature.boneCount > 0u)
    {
        std::vector<Node> nodes;
        std::vector<string> nodeIds;
        for (uint16_t i = 0; i < Armature.boneCount; i++)
        {
            Node node;
            node.name = Armature.boneNames[i];
            node.matrix = Microsoft::glTF::Matrix4();
            for (size_t r = 0; r < 4; r++)
            {
                for (size_t c = 0; c < 4; c++)
                {
                    node.matrix.values[r * 4 + c] = Armature.matrix[i][r][c];
                }
            }
            nodes.push_back(node);
            if (Armature.boneParents[i] > -1)
                nodes[Armature.boneParents[i]].children.push_back(std::to_string(i));
        }
        for (uint16_t i = 0; i < Armature.boneCount; i++)
        {
            nodeIds.push_back(document.nodes.Append(std::move(nodes[i]), AppendIdPolicy::GenerateOnEmpty).id);
        }
        scene.nodes.push_back(nodeIds[0]);
        Skin skin;
        skin.jointIds = nodeIds;

        // Create a BufferView with target ARRAY_BUFFER (as it will reference IBM'sdata)
        bufferBuilder.AddBufferView();

        std::vector<float> IBMs;
        for (uint16_t i = 0; i < Armature.boneCount; i++)
        {

            for (size_t r = 0; r < 4; r++)
            {
                for (size_t c = 0; c < 4; c++)
                {
                    IBMs.push_back(Armature.IBMs[i][r][c]);
                }
            }
        }

        skin.inverseBindMatricesAccessorId = bufferBuilder.AddAccessor(IBMs, { TYPE_MAT4, COMPONENT_FLOAT }).id;

        document.skins.Append(std::move(skin), AppendIdPolicy::GenerateOnEmpty);
        for (size_t i = 0; i < expMeshes.size(); i++)
        {
            scene.nodes.push_back(AddMesh(document, bufferBuilder, expMeshes[i],true));
        }
    }
    else
    {
        for (int i = 0; i < expMeshes.size(); i++)
        {
            scene.nodes.push_back(AddMesh(document, bufferBuilder, expMeshes[i],false));
        }
    }
    // Add it to the Document, using a utility method that also sets the Scene as the Document's default
    document.SetDefaultScene(std::move(scene), AppendIdPolicy::GenerateOnEmpty);

    // Add all of the Buffers, BufferViews and Accessors that were created using BufferBuilder to
    // the Document. Note that after this point, no further calls should be made to BufferBuilder
    bufferBuilder.Output(document);

    std::string manifest;

    try
    {
        // Serialize the glTF Document into a JSON manifest
        manifest = Serialize(document, SerializeFlags::Pretty);
    }
    catch (const GLTFException& ex)
    {
        std::stringstream ss;

        ss << "Microsoft::glTF::Serialize failed: ";
        ss << ex.what();

        throw std::runtime_error(ss.str());
    }
    auto& gltfResourceWriter = bufferBuilder.GetResourceWriter();

    if (auto glbResourceWriter = dynamic_cast<GLBResourceWriter*>(&gltfResourceWriter))
    {
        glbResourceWriter->Flush(manifest, pathFile.string()); // A GLB container isn't created until the GLBResourceWriter::Flush member function is called
    }
    else
    {
        gltfResourceWriter.WriteExternal(pathFile.string(), manifest); // Binary resources have already been written, just need to write the manifest
    }
}