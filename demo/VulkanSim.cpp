#include "Vulkan.h"

#include "Engine.h"
#include <glm/gtx/hash.hpp>
#include <mshio/mshio.h>
#include <random>
#include <unordered_set>

using namespace glm;
using namespace vk;

void Vulkan::generateStarParticles()
{
    std::random_device rd{};
    std::mt19937 gen{rd()};
    std::normal_distribution<float> normDist{0.0f, 1.0f};

    storage.x.reserve(starParticleCount);
    storage.v.reserve(starParticleCount);
    storage.r.reserve(starParticleCount);
    storage.w.reserve(starParticleCount);
    storage.state.reserve(starParticleCount);
    for (uint32_t i = 0; i < starParticleCount; i++)
    {
        // Generate particles around the moon at 5m height.
        float3 x;
        do
        {
            x = {normDist(gen), normDist(gen), normDist(gen)};
        } while (abs(x.x) < 1e-5f && abs(x.y) < 1e-5f && abs(x.z) < 1e-5f);
        x = normalize(x);
        storage.x.emplace_back(float4{25.0f * x, 1.0f});

        // Generate particle velocities pointing towards the moon.
        float3 v;
        do
        {
            v = {normDist(gen), normDist(gen), normDist(gen)};
            v = normalize(v);
        } while (dot(x, v) >= 0.0f);
        storage.v.emplace_back(float4{3.0f * v, 0.0f});

        storage.r.emplace_back(starParticleRadius);
        storage.w.emplace_back(0.001f);
        storage.state.emplace_back(static_cast<glm::uint>(State::STATIC));
    }
}

MeshEmbedding Vulkan::loadMesh(const std::string& model, const std::string& mesh, float compliance, float density,
                               const std::vector<uint32_t>& staticNodes, const glm::float4x4& transformation)
{
    static constexpr float sixth = 1.0f / 6.0f;

    Mesh data{};

    // Load the mesh specification.
    const std::string path = modelMeshPath(model, mesh).string();
    const auto spec = mshio::load_msh(path);
    if (spec.nodes.num_entity_blocks != 1 || spec.elements.num_entity_blocks != 1)
    {
        throw std::runtime_error("Failed to load mesh [" + mesh + "] of model [" + model +
                                 "]: unsupported entity block count");
    }

    // Determine whether the mesh is triangular or tetrahedral.
    const auto& nodeBlock = spec.nodes.entity_blocks[0];
    const auto& elementBlock = spec.elements.entity_blocks[0];
    const int elementDim = mshio::get_element_dim(elementBlock.element_type);
    const size_t elementNodeCount = mshio::nodes_per_element(elementBlock.element_type);
    if (elementDim == 2 && elementNodeCount == 3)
    {
        data.tet = false;
    }
    else if (elementDim == 3 && elementNodeCount == 4)
    {
        data.tet = true;
    }
    else
    {
        throw std::runtime_error("Failed to load mesh [" + mesh + "] of model [" + model +
                                 "]: unsupported element type");
    }

    // Initialize the nodes.
    const auto& nodeTags = nodeBlock.tags;
    const auto& nodeData = nodeBlock.data;
    const size_t nodeCount = nodeBlock.num_nodes_in_block;
    std::unordered_map<size_t, size_t> nodeTagsToIndices{};
    nodeTagsToIndices.reserve(nodeCount);
    const size_t nodeOffset = storage.x.size();
    storage.x.reserve(nodeOffset + nodeCount);
    storage.v.reserve(nodeOffset + nodeCount);
    storage.r.reserve(nodeOffset + nodeCount);
    storage.w.reserve(nodeOffset + nodeCount);
    storage.state.reserve(nodeOffset + nodeCount);
    for (size_t i = 0; i < nodeCount; i++)
    {
        nodeTagsToIndices[nodeTags[i]] = nodeOffset + i;
        storage.x.emplace_back(
            transformPoint({nodeData[3 * i], nodeData[3 * i + 1], nodeData[3 * i + 2]}, transformation));
        storage.v.emplace_back(float4{});
        storage.r.emplace_back(std::numeric_limits<float>::max());
        storage.w.emplace_back(0.0f);
        storage.state.emplace_back(static_cast<glm::uint>(State::FREE));
    }

    // Initialize the elements.
    const auto& elementData = elementBlock.data;
    const size_t elementCount = elementBlock.num_elements_in_block;
    data.elements.reserve(elementCount);
    std::unordered_set<uvec2> edges{};
    std::unordered_multimap<uvec2, uint32_t> edgesToApices{};
    if (data.tet)
    {
        storage.volConstr.reserve(storage.volConstr.size() + elementCount);
    }
    else
    {
        edgesToApices.reserve(3 * elementCount);
    }
    for (size_t i = 0; i < elementCount; i++)
    {
        if (data.tet)
        {
            data.elements.emplace_back(uvec4{
                nodeTagsToIndices[elementData[5 * i + 1]],
                nodeTagsToIndices[elementData[5 * i + 2]],
                nodeTagsToIndices[elementData[5 * i + 3]],
                nodeTagsToIndices[elementData[5 * i + 4]],
            });
            const uvec4& element = data.elements.back();
            const uint32_t _i = element[0];
            const uint32_t _j = element[1];
            const uint32_t _k = element[2];
            const uint32_t _l = element[3];

            // Collect the edges.
            edges.emplace(uvec2{std::min(_i, _j), std::max(_i, _j)});
            edges.emplace(uvec2{std::min(_i, _k), std::max(_i, _k)});
            edges.emplace(uvec2{std::min(_i, _l), std::max(_i, _l)});
            edges.emplace(uvec2{std::min(_j, _k), std::max(_j, _k)});
            edges.emplace(uvec2{std::min(_j, _l), std::max(_j, _l)});
            edges.emplace(uvec2{std::min(_k, _l), std::max(_k, _l)});

            // Generate a volume constraint.
            const float3& x_i = storage.x[_i];
            const float3& x_j = storage.x[_j];
            const float3& x_k = storage.x[_k];
            const float3& x_l = storage.x[_l];
            float V = sixth * dot(cross(x_j - x_i, x_k - x_i), x_l - x_i);
            storage.volConstr.emplace_back(VolumeConstraint{
                .i = _i,
                .j = _j,
                .k = _k,
                .l = _l,
                .V = V,
                .alpha = compliance,
            });

            // Collect the volumes of the adjacent elements for each node.
            V = std::abs(V);
            storage.w[_i] += V;
            storage.w[_j] += V;
            storage.w[_k] += V;
            storage.w[_l] += V;
        }
        else
        {
            data.elements.emplace_back(uvec4{
                nodeTagsToIndices[elementData[4 * i + 1]],
                nodeTagsToIndices[elementData[4 * i + 2]],
                nodeTagsToIndices[elementData[4 * i + 3]],
                nodeTagsToIndices[elementData[4 * i + 1]],
            });
            const uvec4& element = data.elements.back();
            const uint32_t _i = element[0];
            const uint32_t _j = element[1];
            const uint32_t _k = element[2];

            // Collect the edges and the apices for each edge.
            const uvec2 e_ij{std::min(_i, _j), std::max(_i, _j)};
            const uvec2 e_ik{std::min(_i, _k), std::max(_i, _k)};
            const uvec2 e_jk{std::min(_j, _k), std::max(_j, _k)};
            edges.emplace(e_ij);
            edges.emplace(e_ik);
            edges.emplace(e_jk);
            edgesToApices.emplace(e_ij, _k);
            edgesToApices.emplace(e_ik, _j);
            edgesToApices.emplace(e_jk, _i);

            // Collect the areas of the adjacent elements for each node.
            const float3& x_i = storage.x[_i];
            const float3& x_j = storage.x[_j];
            const float3& x_k = storage.x[_k];
            const float A = 0.5f * length(cross(x_j - x_i, x_k - x_i));
            storage.w[_i] += A;
            storage.w[_j] += A;
            storage.w[_k] += A;
        }
    }

    // Generate stretching distance constraints between adjacent nodes.
    storage.distConstr.reserve(storage.distConstr.size() + edges.size());
    for (const uvec2& edge : edges)
    {
        const uint32_t _i = edge[0];
        const uint32_t _j = edge[1];
        const float3& x_i = storage.x[_i];
        const float3& x_j = storage.x[_j];
        const float d = distance(x_i, x_j);
        data.d_mean += d;
        const float r = 0.5f * d;
        // Choose the minimum radius to prevent jittering between collision and distance constraints.
        storage.r[_i] = std::min(storage.r[_i], r);
        storage.r[_j] = std::min(storage.r[_j], r);

        storage.distConstr.emplace_back(DistanceConstraint{
            .i = _i,
            .j = _j,
            .d = d,
            // Handle triangle meshes as quasi non-stretchable cloth.
            .alpha = data.tet ? compliance : 0.0f,
        });
    }
    data.d_mean /= static_cast<float>(edges.size());

    if (!data.tet)
    {
        // Generate bending distance constraints between apices of shared edges.
        storage.distConstr.reserve(storage.distConstr.size() + edges.size());
        auto it = edgesToApices.begin();
        while (it != edgesToApices.end())
        {
            const uint32_t _i = it->second;
            const float3& x_i = storage.x[_i];
            auto it_ = std::next(it);
            while (it_ != edgesToApices.end() && it_->first == it->first)
            {
                const uint32_t _j = it_->second;
                const float3& x_j = storage.x[_j];
                storage.distConstr.emplace_back(DistanceConstraint{
                    .i = _i,
                    .j = _j,
                    .d = distance(x_i, x_j),
                    .alpha = compliance,
                });
                it_++;
            }
            it = it_;
        }
    }

    for (size_t i = nodeOffset; i < nodeOffset + nodeCount; i++)
    {
        // Calculate the inverse masses via mass lumping,
        // i.e. the element masses are evenly distributed among the nodes.
        if (data.tet)
        {
            storage.w[i] = 4.0f / (density * storage.w[i]);
        }
        else
        {
            storage.w[i] = 3.0f / (density * storage.w[i]);
        }

        // Calculate the maximum radius.
        data.r_max = std::max(data.r_max, storage.r[i]);
    }

    // Set the correct state for the static nodes.
    for (const uint32_t tag : staticNodes)
    {
        storage.state[nodeTagsToIndices[tag]] = static_cast<glm::uint>(State::STATIC);
    }

    // Obtain the indices of the attached nodes.
    if (model == "flag" && mesh == "flag")
    {
        attachmentIndices[0] = nodeTagsToIndices[attachmentIndices[0]];
        attachmentIndices[1] = nodeTagsToIndices[attachmentIndices[1]];
    }

    // Store the mesh data.
    _meshes[model + "/" + mesh] = data;

    return data.tet ? MeshEmbedding::tetrahedral : MeshEmbedding::triangular;
}

void Vulkan::embedMesh(const std::string& model, const std::string& mesh, Data positionData, Data& jointData,
                       Data& weightData)
{
    // Precompute cell neighbors in a spatial grid sorted by the Manhattan distance.
    static const std::multimap<uint32_t, ivec3> distancesToNeighbors = []() -> std::multimap<uint32_t, ivec3> {
        std::multimap<uint32_t, ivec3> distancesToNeighbors;
        for (int i = -5; i <= 5; i++)
        {
            for (int j = -5; j <= 5; j++)
            {
                for (int k = -5; k <= 5; k++)
                {
                    distancesToNeighbors.emplace(std::abs(i) + std::abs(j) + std::abs(k), ivec3{i, j, k});
                }
            }
        }
        return distancesToNeighbors;
    }();

    // Load the mesh data.
    const Mesh& data = _meshes[model + "/" + mesh];

    // Allocate memory for the vertex skinning data.
    const uint32_t vertexCount = positionData.size / sizeof(float3);
    jointData = Data::allocate(vertexCount * sizeof(uvec4));
    weightData = Data::allocate(vertexCount * sizeof(float4));

    // Build a spatial grid from the elements.
    const float cellLength = 1.5f * data.d_mean;
    std::unordered_multimap<ivec3, uint32_t> cellsToElements{};
    cellsToElements.reserve(data.elements.size());
    for (uint32_t i = 0; i < data.elements.size(); i++)
    {
        const uvec4& element = data.elements[i];

        // Determine the axis-aligned bounding box of the element.
        const float3& x_i = storage.x[element[0]];
        const float3& x_j = storage.x[element[1]];
        const float3& x_k = storage.x[element[2]];
        float3 x_min = min(min(x_i, x_j), x_k);
        float3 x_max = max(max(x_i, x_j), x_k);
        if (data.tet)
        {
            const float3& x_l = storage.x[element[3]];
            x_min = min(x_min, x_l);
            x_max = max(x_max, x_l);
        }

        // Add the element to all covered cells of the grid.
        const ivec3 c_min = floor(x_min / cellLength);
        const ivec3 c_max = floor(x_max / cellLength);
        for (int cx = c_min.x; cx <= c_max.x; cx++)
        {
            for (int cy = c_min.y; cy <= c_max.y; cy++)
            {
                for (int cz = c_min.z; cz <= c_max.z; cz++)
                {
                    cellsToElements.emplace(ivec3{cx, cy, cz}, i);
                }
            }
        }
    }

    // Find an embedding for the vertices of the surface mesh.
    const std::vector<Model::Node*>& modelMeshNodes = getModel(model).mesh(mesh).nodes;
    if (modelMeshNodes.size() != 1)
    {
        throw std::runtime_error("Failed to embed mesh [" + mesh + "] of model [" + model +
                                 "]: unsupported node count");
    }
    const float4x4& transformation = modelMeshNodes[0]->model;
    const std::span positions{static_cast<float3*>(positionData()), vertexCount};
    std::span joints{static_cast<uvec4*>(jointData()), vertexCount};
    std::span weights{static_cast<float4*>(weightData()), vertexCount};
    std::vector<float> distances(vertexCount, std::numeric_limits<float>::max());
    for (uint32_t vertex = 0; vertex < vertexCount; vertex++)
    {
        // Calculate the cell index for the vertex.
        const float3 x = transformPoint(positions[vertex], transformation);
        const ivec3 c0 = floor(x / cellLength);

        // Search around the cell for the nearest element.
        uint32_t cellDistance = 0;
        bool foundElement = false;
        bool foundEnclosingElement = false;
        for (const auto& distanceToNeighbor : distancesToNeighbors)
        {
            if (foundElement && cellDistance != distanceToNeighbor.first)
            {
                // Search all cells of the same distance to find the nearest element if no enclosing element exists.
                break;
            }
            cellDistance = distanceToNeighbor.first;
            const ivec3 c = c0 + distanceToNeighbor.second;
            if (cellsToElements.contains(c))
            {
                foundElement = true;
                auto range = cellsToElements.equal_range(c);
                for (auto it = range.first; it != range.second; it++)
                {
                    const uvec4& element = data.elements[it->second];

                    // Calculate the barycentric coordinates of the vertex wrt the element.
                    float4 b;
                    const float3& x_i = storage.x[element[0]];
                    const float3& x_j = storage.x[element[1]];
                    const float3& x_k = storage.x[element[2]];
                    const float3 x__i = x - x_i;
                    const float3 x_ji = x_j - x_i;
                    const float3 x_ki = x_k - x_i;
                    const float3 n = cross(x_ji, x_ki);
                    if (data.tet)
                    {
                        const float3 x_l = storage.x[element[3]];
                        const float3 x_li = x_l - x_i;
                        const float invAbs6V = 1.0f / std::abs(dot(n, x_li));
                        b = {
                            dot(cross(x_l - x_j, x_k - x_j), x - x_j) * invAbs6V,
                            dot(cross(x_ki, x_li), x__i) * invAbs6V,
                            dot(cross(x_li, x_ji), x__i) * invAbs6V,
                            dot(n, x__i) * invAbs6V,
                        };
                    }
                    else
                    {
                        const float invSq2A = 1.0f / dot(n, n);
                        b = {
                            dot(cross(x_k - x_j, x - x_j), n) * invSq2A,
                            dot(cross(x__i, x_ki), n) * invSq2A,
                            dot(cross(x_ji, x__i), n) * invSq2A,
                            0.0f,
                        };
                    }

                    // Calculate the barycentric distance.
                    const float d = data.tet ? std::max(std::max(std::max(-b[0], -b[1]), -b[2]), -b[3])
                                             : std::max(std::max(-b[0], -b[1]), -b[2]);
                    if (d < distances[vertex])
                    {
                        // Update the vertex skinning data for the nearer element.
                        distances[vertex] = d;
                        joints[vertex] = element - starParticleCount;
                        weights[vertex] = b;
                    }
                    if (d <= 0.0f)
                    {
                        // All coordinates are non-negative, i.e. the element encloses the vertex.
                        foundEnclosingElement = true;
                        break;
                    }
                }
                if (foundEnclosingElement)
                {
                    // Terminate the search early.
                    break;
                }
            }
        }
        if (!foundElement)
        {
            throw std::runtime_error("Failed to embed mesh [" + mesh + "] of model [" + model +
                                     "]: vertex too far away");
        }
    }
}

void Vulkan::initializeSimulation()
{
    // Assert that the star particle count is a multiple of 64,
    // so that even the smallest used storage type with size of 4 bytes
    // is aligned to the max. storage offset alignment of 256 bytes.
    static_assert(alignedSize(starParticleCount, 64) == starParticleCount);

    // Initialize the entity counts.
    particleCount = storage.x.size();
    spatCount = std::bit_ceil(particleCount);
    distCount = storage.distConstr.size();
    volCount = storage.volConstr.size();

    // Select the workgroup dimensions.
    starWorkgroup = gpu.selectWorkgroupDimensions(starParticleCount, 256, 0);
    particleWorkgroup = gpu.selectWorkgroupDimensions(particleCount, 256, 0);
    spatWorkgroup = gpu.selectWorkgroupDimensions(spatCount, 256, 0);
    sharedSpatWorkgroup = gpu.selectWorkgroupDimensions(spatCount, -1, sizeof(uvec2));
    distWorkgroup = gpu.selectWorkgroupDimensions(distCount, 256, 0);
    volWorkgroup = gpu.selectWorkgroupDimensions(volCount, 256, 0);

    // Initialize the maximum particle radius and the cell size.
    r_max = starParticleRadius;
    for (const auto& idToMesh : _meshes)
    {
        r_max = std::max(r_max, idToMesh.second.r_max);
    }
    cellSize = 2.0f * r_max;

    // Destroy the mesh data.
    _meshes.clear();

    // Calculate and reserve storage buffer sizes.
    storage.size.x = particleCount * sizeof(float4);
    storage.offset.x = storageBufferSize;
    storageBufferSize += gpu.alignedStorageSize(storage.size.x);
    storage.size.x_ = particleCount * sizeof(float4);
    storage.offset.x_ = storageBufferSize;
    storageBufferSize += gpu.alignedStorageSize(storage.size.x_);
    storage.size.dx = particleCount * sizeof(float4);
    storage.offset.dx = storageBufferSize;
    storageBufferSize += gpu.alignedStorageSize(storage.size.dx);
    storage.size.dxE7 = particleCount * sizeof(int4);
    storage.offset.dxE7 = storageBufferSize;
    storageBufferSize += gpu.alignedStorageSize(storage.size.dxE7);
    storage.size.v = particleCount * sizeof(float4);
    storage.offset.v = storageBufferSize;
    storageBufferSize += gpu.alignedStorageSize(storage.size.v);
    storage.size.spat = spatCount * sizeof(Spatial);
    storage.offset.spat = storageBufferSize;
    storageBufferSize += gpu.alignedStorageSize(storage.size.spat);
    storage.size.cell = particleCount * sizeof(uvec2);
    storage.offset.cell = storageBufferSize;
    storageBufferSize += gpu.alignedStorageSize(storage.size.cell);
    storage.size.r = particleCount * sizeof(float);
    storage.offset.r = storageBufferSize;
    storageBufferSize += gpu.alignedStorageSize(storage.size.r);
    storage.size.w = particleCount * sizeof(float);
    storage.offset.w = storageBufferSize;
    storageBufferSize += gpu.alignedStorageSize(storage.size.w);
    storage.size.state = particleCount * sizeof(glm::uint);
    storage.offset.state = storageBufferSize;
    storageBufferSize += gpu.alignedStorageSize(storage.size.state);
    storage.size.distConstr = distCount * sizeof(DistanceConstraint);
    storage.offset.distConstr = storageBufferSize;
    storageBufferSize += gpu.alignedStorageSize(storage.size.distConstr);
    storage.size.volConstr = volCount * sizeof(VolumeConstraint);
    storage.offset.volConstr = storageBufferSize;
    storageBufferSize += gpu.alignedStorageSize(storage.size.volConstr);

    // Initialize the storage buffer.
    setupTransfer();
    storageBuffer =
        createBuffer(storageBufferSize, BufferUsageFlagBits::eStorageBuffer | BufferUsageFlagBits::eTransferDst);
    fillBuffer(storageBuffer, Data::of(storage.x), storage.offset.x);
    fillBuffer(storageBuffer, Data::of(storage.v), storage.offset.v);
    clearBuffer(storageBuffer, ~0, storage.offset.spat, storage.size.spat);
    fillBuffer(storageBuffer, Data::of(storage.r), storage.offset.r);
    fillBuffer(storageBuffer, Data::of(storage.w), storage.offset.w);
    fillBuffer(storageBuffer, Data::of(storage.state), storage.offset.state);
    fillBuffer(storageBuffer, Data::of(storage.distConstr), storage.offset.distConstr);
    fillBuffer(storageBuffer, Data::of(storage.volConstr), storage.offset.volConstr);
    playTransfer();

    // Destroy the initial storage data.
    storage.x.clear();
    storage.v.clear();
    storage.r.clear();
    storage.w.clear();
    storage.distConstr.clear();
    storage.volConstr.clear();

    // Initialize the attachment copies.
    for (uint32_t i = 0; i < attachmentCount; i++)
    {
        attachmentCopies[i] = BufferCopy{
            .srcOffset = attachmentOffset + i * sizeof(float4),
            .dstOffset = storage.offset.x + attachmentIndices[i] * sizeof(float4),
            .size = sizeof(float3),
        };
    }

    // Initialize the counter buffer.
    counterBufferSize = gpu.alignedStorageSize(sizeof(glm::uint));
    counterBuffer = createReadbackBuffer(counterBufferSize, BufferUsageFlagBits::eStorageBuffer);
    mapBuffer(counterBuffer);
    counterBuffer.as<glm::uint>() = 0;

    Model& astronaut = getModel("astronaut");

    // Get the nodes and initialize the uniform data for the player collision.
    constexpr std::array nodes{
        "Spine_1.6_9",   "head.42_45",                                         // torso
        "L_Arm.10_13",   "L_Elbow.11_14", "L_Wrist.12_15", "L_middle_1.19_22", // left arm
        "R_Arm.44_47",   "R_Elbow.45_48", "R_Wrist.46_49", "R_middle_1.53_56", // right arm
        "L_Thigh.82_85", "L_Knee.83_86",  "L_Ankle.84_87", "L_Toe.85_88",      // left leg
        "R_Thigh.88_91", "R_Knee.89_92",  "R_Ankle.90_93", "R_Toe.91_94",      // right leg
    };
    constexpr std::array radii{
        0.5f,  0.5f,                // torso
        0.18f, 0.14f, 0.12f, 0.16f, // left arm
        0.18f, 0.14f, 0.12f, 0.16f, // right arm
        0.25f, 0.23f, 0.21f, 0.21f, // left leg
        0.25f, 0.23f, 0.21f, 0.21f, // right leg
    };
    constexpr std::array capsules{
        uvec2{0, 1},                                 // torso
        uvec2{2, 3},   uvec2{3, 4},   uvec2{4, 5},   // left arm
        uvec2{6, 7},   uvec2{7, 8},   uvec2{8, 9},   // right arm
        uvec2{10, 11}, uvec2{11, 12}, uvec2{12, 13}, // left leg
        uvec2{14, 15}, uvec2{15, 16}, uvec2{16, 17}, // right leg
    };
    for (uint32_t i = 0; i < nodes.size(); i++)
    {
        playerCollisionNodes[i] = &astronaut.node(nodes[i]);
        playerCollision.sphere[i][3] = radii[i];
    }
    for (uint32_t i = 0; i < capsules.size(); i++)
    {
        playerCollision.caps[i][0] = capsules[i].x;
        playerCollision.caps[i][1] = capsules[i].y;
    }

    // Get the player attachment nodes.
    playerAttachmentNodes[0] = &astronaut.node("L_bagPackHandle.77_80");
    playerAttachmentNodes[1] = &astronaut.node("R_bagPackHandle.78_81");
}

void Vulkan::updatePlayer()
{
    // Update the player model.
    Model& astronaut = getModel("astronaut");
    astronaut.translation = engine.player.x;
    astronaut.rotation = engine.player.q;
    if (engine.state == Engine::State::Main && engine.player.isMoving())
    {
        const float deltaTime = (engine.player.runs ? 2.0f : 1.0f) * engine.deltaTime;
        astronaut.animation("moon_walk").play(deltaTime, true);
    }
    else if (engine.state == Engine::State::Finale)
    {
        astronaut.animation("wave").play(engine.deltaTime, true);
    }
    else
    {
        astronaut.animation("idle").play(engine.deltaTime, true);
    }
    astronaut.update();

    // Update the player collision.
    for (uint32_t i = 0; i < playerCollision.sphere.size(); i++)
    {
        // Update the bounding sphere positions.
        const Model::Node& node = *playerCollisionNodes[i];
        playerCollision.sphere[i][0] = node.model[3][0];
        playerCollision.sphere[i][1] = node.model[3][1];
        playerCollision.sphere[i][2] = node.model[3][2];
    }
    playerCollision.x_min[0] = std::numeric_limits<float>::max();
    playerCollision.x_min[1] = std::numeric_limits<float>::max();
    playerCollision.x_min[2] = std::numeric_limits<float>::max();
    playerCollision.x_max[0] = -std::numeric_limits<float>::max();
    playerCollision.x_max[1] = -std::numeric_limits<float>::max();
    playerCollision.x_max[2] = -std::numeric_limits<float>::max();
    for (uint32_t i = 0; i < playerCollision.caps.size(); i++)
    {
        const float4& a = playerCollision.sphere[playerCollision.caps[i].x];
        const float4& b = playerCollision.sphere[playerCollision.caps[i].y];

        // Update the AABBs.
        playerCollision.c_min[i][0] = std::min(a.x - a.w, b.x - b.w);
        playerCollision.c_min[i][1] = std::min(a.y - a.w, b.y - b.w);
        playerCollision.c_min[i][2] = std::min(a.z - a.w, b.z - b.w);
        playerCollision.c_max[i][0] = std::max(a.x + a.w, b.x + b.w);
        playerCollision.c_max[i][1] = std::max(a.y + a.w, b.y + b.w);
        playerCollision.c_max[i][2] = std::max(a.z + a.w, b.z + b.w);
        playerCollision.x_min[0] = std::min(playerCollision.x_min[0], playerCollision.c_min[i][0]);
        playerCollision.x_min[1] = std::min(playerCollision.x_min[1], playerCollision.c_min[i][1]);
        playerCollision.x_min[2] = std::min(playerCollision.x_min[2], playerCollision.c_min[i][2]);
        playerCollision.x_max[0] = std::max(playerCollision.x_max[0], playerCollision.c_max[i][0]);
        playerCollision.x_max[1] = std::max(playerCollision.x_max[1], playerCollision.c_max[i][1]);
        playerCollision.x_max[2] = std::max(playerCollision.x_max[2], playerCollision.c_max[i][2]);
    }
    varUniformBuffers[updateIndex].set(playerCollision, playerCollisionUniformOffset);

    // Update the player attachments.
    for (uint32_t i = 0; i < attachmentCount; i++)
    {
        const Model::Node& node = *playerAttachmentNodes[i];
        attachmentPositions[i] = node.model * attachmentDeltas[i];
    }
    varUniformBuffers[updateIndex].set(attachmentPositions, attachmentOffset);
}

void Vulkan::recordSimulation(vk::CommandBuffer& simBuffer)
{
    simBuffer.begin(CommandBufferBeginInfo{});
    activate(simBuffer);

    // Activate the star particles at the beginning of the main state.
    if (!starParticlesActive && engine.state == Engine::State::Main)
    {
        clearBuffer(storageBuffer, static_cast<uint32_t>(State::FREE), storage.offset.state,
                    starParticleCount * sizeof(glm::uint));
        syncBufferAccess(storageBuffer, PipelineStageFlagBits::eTransfer, AccessFlagBits::eTransferWrite,
                         PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.state,
                         storage.size.state);
        starParticlesActive = true;
    }

    // Let the star attract all star particles from the beginning of the finale state.
    if (!attractAllStarParticles && engine.state == Engine::State::Finale)
    {
        clearBuffer(storageBuffer, static_cast<uint32_t>(State::STAR), storage.offset.state,
                    starParticleCount * sizeof(glm::uint));
        syncBufferAccess(storageBuffer, PipelineStageFlagBits::eTransfer, AccessFlagBits::eTransferWrite,
                         PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.state,
                         storage.size.state);
        attractAllStarParticles = true;
    }

    // Record the star update pass.
    simBuffer.bindPipeline(PipelineBindPoint::eCompute, starUpdatePipeline);
    simBuffer.bindDescriptorSets(PipelineBindPoint::eCompute, starUpdatePipelineLayout, 0,
                                 starUpdateDescSets[updateIndex], {});
    simBuffer.pushConstants<float4>(starUpdatePipelineLayout, ShaderStageFlagBits::eCompute, 0,
                                    float4(engine.player.x, 1.0));
    simBuffer.pushConstants<glm::uint>(starUpdatePipelineLayout, ShaderStageFlagBits::eCompute, sizeof(float4),
                                       starParticleCount);
    simBuffer.dispatch(starWorkgroup.count, 1, 1);

    // Update the positions of the attached particles.
    simBuffer.copyBuffer(varUniformBuffers[updateIndex](), storageBuffer(), attachmentCopies);

    // Record the spatial hash pass.
    syncBufferAccess(storageBuffer, PipelineStageFlagBits::eComputeShader | PipelineStageFlagBits::eTransfer,
                     AccessFlagBits::eShaderWrite | AccessFlagBits::eTransferWrite,
                     PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.x,
                     storage.size.x);
    syncBufferAccess(storageBuffer, PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderWrite,
                     PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.v,
                     storage.size.v);
    simBuffer.bindPipeline(PipelineBindPoint::eCompute, spatialHashPipeline);
    simBuffer.bindDescriptorSets(PipelineBindPoint::eCompute, spatialHashPipelineLayout, 0,
                                 spatialHashDescSets[updateIndex], {});
    simBuffer.pushConstants<float>(spatialHashPipelineLayout, ShaderStageFlagBits::eCompute, 0, engine.deltaTime);
    simBuffer.pushConstants<float>(spatialHashPipelineLayout, ShaderStageFlagBits::eCompute, sizeof(float),
                                   engine.gravity);
    simBuffer.pushConstants<float>(spatialHashPipelineLayout, ShaderStageFlagBits::eCompute, 2 * sizeof(float),
                                   cellSize);
    simBuffer.pushConstants<glm::uint>(spatialHashPipelineLayout, ShaderStageFlagBits::eCompute, 3 * sizeof(float),
                                       particleCount);
    simBuffer.dispatch(particleWorkgroup.count, 1, 1);

    // Record the spatial sort passes.
    for (uint32_t peak = 2; peak <= spatCount; peak *= 2)
    {
        for (uint32_t dist = peak >> 1; dist > 0; dist >>= 1)
        {
            syncBufferAccess(storageBuffer, PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderWrite,
                             PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.spat,
                             storage.size.spat);
            if (dist < sharedSpatWorkgroup.size)
            {
                // Perform the rest of the subiterations in a single pass using group-shared memory.
                simBuffer.bindPipeline(PipelineBindPoint::eCompute, spatialGroupsortPipeline);
                simBuffer.bindDescriptorSets(PipelineBindPoint::eCompute, spatialGroupsortPipelineLayout, 0,
                                             spatialGroupsortDescSets[updateIndex], {});
                simBuffer.pushConstants<glm::uint>(spatialGroupsortPipelineLayout, ShaderStageFlagBits::eCompute, 0,
                                                   peak);
                simBuffer.pushConstants<glm::uint>(spatialGroupsortPipelineLayout, ShaderStageFlagBits::eCompute,
                                                   sizeof(glm::uint), dist);
                simBuffer.dispatch(sharedSpatWorkgroup.count, 1, 1);
                break;
            }
            else
            {
                simBuffer.bindPipeline(PipelineBindPoint::eCompute, spatialSortPipeline);
                simBuffer.bindDescriptorSets(PipelineBindPoint::eCompute, spatialSortPipelineLayout, 0,
                                             spatialSortDescSets[updateIndex], {});
                simBuffer.pushConstants<glm::uint>(spatialSortPipelineLayout, ShaderStageFlagBits::eCompute, 0, peak);
                simBuffer.pushConstants<glm::uint>(spatialSortPipelineLayout, ShaderStageFlagBits::eCompute,
                                                   sizeof(glm::uint), dist);
                simBuffer.dispatch(spatWorkgroup.count, 1, 1);
            }
        }
    }

    // Record the spatial collect pass.
    syncBufferAccess(storageBuffer, PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderWrite,
                     PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.spat,
                     storage.size.spat);
    simBuffer.bindPipeline(PipelineBindPoint::eCompute, spatialCollectPipeline);
    simBuffer.bindDescriptorSets(PipelineBindPoint::eCompute, spatialCollectPipelineLayout, 0,
                                 spatialCollectDescSets[updateIndex], {});
    simBuffer.pushConstants<glm::uint>(spatialCollectPipelineLayout, ShaderStageFlagBits::eCompute, 0, particleCount);
    simBuffer.dispatch(particleWorkgroup.count, 1, 1);

    for (uint32_t i = 0; i < substepCount; i++)
    {
        // Clear the position deltas.
        if (i != 0)
        {
            syncBufferAccess(storageBuffer, PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead,
                             PipelineStageFlagBits::eTransfer, AccessFlagBits::eTransferWrite, storage.offset.dx,
                             storage.size.dx);
        }
        clearBuffer(storageBuffer, 0, storage.offset.dx, storage.size.dx);
        if (i != 0)
        {
            syncBufferAccess(storageBuffer, PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead,
                             PipelineStageFlagBits::eTransfer, AccessFlagBits::eTransferWrite, storage.offset.dxE7,
                             storage.size.dxE7);
        }
        clearBuffer(storageBuffer, 0, storage.offset.dxE7, storage.size.dxE7);

        // Record the XPBD predict pass.
        if (i != 0)
        {
            syncBufferAccess(storageBuffer, PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderWrite,
                             PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.x,
                             storage.size.x);
            syncBufferAccess(storageBuffer, PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderWrite,
                             PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.v,
                             storage.size.v);
        }
        simBuffer.bindPipeline(PipelineBindPoint::eCompute, xpbdPredictPipeline);
        simBuffer.bindDescriptorSets(PipelineBindPoint::eCompute, xpbdPredictPipelineLayout, 0,
                                     xpbdPredictDescSets[updateIndex], {});
        simBuffer.pushConstants<float>(xpbdPredictPipelineLayout, ShaderStageFlagBits::eCompute, 0, substepDeltaTime);
        simBuffer.pushConstants<float>(xpbdPredictPipelineLayout, ShaderStageFlagBits::eCompute, sizeof(float),
                                       engine.gravity);
        simBuffer.pushConstants<glm::uint>(xpbdPredictPipelineLayout, ShaderStageFlagBits::eCompute, 2 * sizeof(float),
                                           particleCount);
        simBuffer.dispatch(particleWorkgroup.count, 1, 1);

        // Record the XPBD object collide pass.
        syncBufferAccess(storageBuffer, PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderWrite,
                         PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.x_,
                         storage.size.x_);
        syncBufferAccess(storageBuffer, PipelineStageFlagBits::eTransfer, AccessFlagBits::eTransferWrite,
                         PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.dx,
                         storage.size.dx);
        simBuffer.bindPipeline(PipelineBindPoint::eCompute, xpbdObjcollPipeline);
        simBuffer.bindDescriptorSets(PipelineBindPoint::eCompute, xpbdObjcollPipelineLayout, 0,
                                     xpbdObjcollDescSets[updateIndex], {});
        simBuffer.pushConstants<glm::uint>(xpbdObjcollPipelineLayout, ShaderStageFlagBits::eCompute, 0, particleCount);
        simBuffer.dispatch(particleWorkgroup.count, 1, 1);

        // Record the XPBD particle collide pass.
        if (i == 0)
        {
            syncBufferAccess(storageBuffer, PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderWrite,
                             PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.cell,
                             storage.size.cell);
            syncBufferAccess(storageBuffer, PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderWrite,
                             PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.state,
                             storage.size.state);
        }
        syncBufferAccess(storageBuffer, PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderWrite,
                         PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.dx,
                         storage.size.dx);
        simBuffer.bindPipeline(PipelineBindPoint::eCompute, xpbdPcollPipeline);
        simBuffer.bindDescriptorSets(PipelineBindPoint::eCompute, xpbdPcollPipelineLayout, 0,
                                     xpbdPcollDescSets[updateIndex], {});
        simBuffer.pushConstants<glm::uint>(xpbdPcollPipelineLayout, ShaderStageFlagBits::eCompute, 0, particleCount);
        simBuffer.dispatch(particleWorkgroup.count, 1, 1);

        // Record the XPBD distance constrain pass.
        syncBufferAccess(storageBuffer, PipelineStageFlagBits::eTransfer, AccessFlagBits::eTransferWrite,
                         PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.dxE7,
                         storage.size.dxE7);
        simBuffer.bindPipeline(PipelineBindPoint::eCompute, xpbdDistPipeline);
        simBuffer.bindDescriptorSets(PipelineBindPoint::eCompute, xpbdDistPipelineLayout, 0,
                                     xpbdDistDescSets[updateIndex], {});
        simBuffer.pushConstants<float>(xpbdDistPipelineLayout, ShaderStageFlagBits::eCompute, 0, substepDeltaTime);
        simBuffer.pushConstants<glm::uint>(xpbdDistPipelineLayout, ShaderStageFlagBits::eCompute, sizeof(float),
                                           distCount);
        simBuffer.dispatch(distWorkgroup.count, 1, 1);

        // Record the XPBD volume constrain pass.
        simBuffer.bindPipeline(PipelineBindPoint::eCompute, xpbdVolPipeline);
        simBuffer.bindDescriptorSets(PipelineBindPoint::eCompute, xpbdVolPipelineLayout, 0,
                                     xpbdVolDescSets[updateIndex], {});
        simBuffer.pushConstants<float>(xpbdVolPipelineLayout, ShaderStageFlagBits::eCompute, 0, substepDeltaTime);
        simBuffer.pushConstants<glm::uint>(xpbdVolPipelineLayout, ShaderStageFlagBits::eCompute, sizeof(float),
                                           volCount);
        simBuffer.dispatch(volWorkgroup.count, 1, 1);

        // Record the XPBD correct pass.
        syncBufferAccess(storageBuffer, PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderWrite,
                         PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.dx,
                         storage.size.dx);
        syncBufferAccess(storageBuffer, PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderWrite,
                         PipelineStageFlagBits::eComputeShader, AccessFlagBits::eShaderRead, storage.offset.dxE7,
                         storage.size.dxE7);
        simBuffer.bindPipeline(PipelineBindPoint::eCompute, xpbdCorrectPipeline);
        simBuffer.bindDescriptorSets(PipelineBindPoint::eCompute, xpbdCorrectPipelineLayout, 0,
                                     xpbdCorrectDescSets[updateIndex], {});
        simBuffer.pushConstants<float>(xpbdCorrectPipelineLayout, ShaderStageFlagBits::eCompute, 0, substepDeltaTime);
        simBuffer.pushConstants<glm::uint>(xpbdCorrectPipelineLayout, ShaderStageFlagBits::eCompute, sizeof(float),
                                           particleCount);
        simBuffer.dispatch(particleWorkgroup.count, 1, 1);
    }

    simBuffer.end();
}

void Vulkan::sim()
{
    result = device.waitForFences(updateInFlight[updateIndex], true, UINT64_MAX);

    updatePlayer();

    device.resetFences(updateInFlight[updateIndex]);
    device.resetCommandPool(simPools[updateIndex]);

    recordSimulation(simBuffers[updateIndex]);

    const uint64_t waitSemaphoreValue = updateCount;
    updateCount++;
    const uint64_t signalSemaphoreValue = updateCount;
    const TimelineSemaphoreSubmitInfo semaphoreValues{
        .waitSemaphoreValueCount = 1,
        .pWaitSemaphoreValues = &waitSemaphoreValue,
        .signalSemaphoreValueCount = 1,
        .pSignalSemaphoreValues = &signalSemaphoreValue,
    };
    constexpr PipelineStageFlags waitDstStage = PipelineStageFlagBits::eComputeShader;
    computeQueue.submit(
        SubmitInfo{
            .pNext = &semaphoreValues,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &simComplete,
            .pWaitDstStageMask = &waitDstStage,
            .commandBufferCount = 1,
            .pCommandBuffers = &simBuffers[updateIndex],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &simComplete,
        },
        updateInFlight[updateIndex]);

    updateIndex = (updateIndex + 1) % frameCount;
}