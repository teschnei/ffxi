#include "d3m.h"

#include "engine/core.h"
#include "engine/task/particle_model_init.h"

namespace FFXI
{

    std::vector<vk::VertexInputBindingDescription> D3M::Vertex::getBindingDescriptions()
    {
        std::vector<vk::VertexInputBindingDescription> binding_descriptions(1);

        binding_descriptions[0].binding = 0;
        binding_descriptions[0].stride = sizeof(Vertex);
        binding_descriptions[0].inputRate = vk::VertexInputRate::eVertex;

        return binding_descriptions;
    }

    std::vector<vk::VertexInputAttributeDescription> D3M::Vertex::getAttributeDescriptions()
    {
        std::vector<vk::VertexInputAttributeDescription> attribute_descriptions(4);

        attribute_descriptions[0].binding = 0;
        attribute_descriptions[0].location = 0;
        attribute_descriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attribute_descriptions[0].offset = offsetof(Vertex, pos);

        attribute_descriptions[1].binding = 0;
        attribute_descriptions[1].location = 1;
        attribute_descriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attribute_descriptions[1].offset = offsetof(Vertex, normal);

        attribute_descriptions[2].binding = 0;
        attribute_descriptions[2].location = 2;
        attribute_descriptions[2].format = vk::Format::eR8G8B8A8Uint;
        attribute_descriptions[2].offset = offsetof(Vertex, color);

        attribute_descriptions[3].binding = 0;
        attribute_descriptions[3].location = 3;
        attribute_descriptions[3].format = vk::Format::eR32G32Sfloat;
        attribute_descriptions[3].offset = offsetof(Vertex, uv);

        return attribute_descriptions;
    }

    D3M::D3M(char* _name, uint8_t* _buffer, size_t _len) : DatChunk(_name, _buffer, _len)
    {
        assert(*(uint32_t*)buffer == 6);
        //numimg buffer + 0x04
        //numnimg buffer + 0x05
        num_triangles = *(uint16_t*)(buffer + 0x06);
        //numtri1 buffer + 0x08
        //numtri2 buffer + 0x0A
        //numtri3 buffer + 0x0C
        texture_name = std::string((char*)buffer + 0x0E, 16);
        vertex_buffer = (Vertex*)(buffer + 0x1E);
    }

    void D3MLoader::LoadModel(std::shared_ptr<lotus::Model>& model)
    {
        std::vector<uint8_t> vertices(d3m->num_triangles * sizeof(D3M::Vertex) * 3);
        memcpy(vertices.data(), d3m->vertex_buffer, d3m->num_triangles * sizeof(D3M::Vertex) * 3);

        vk::BufferUsageFlags vertex_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;
        vk::BufferUsageFlags index_usage_flags = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer;

        if (engine->renderer.RTXEnabled())
        {
            vertex_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer;
            index_usage_flags |= vk::BufferUsageFlagBits::eStorageBuffer;
        }

        auto mesh = std::make_unique<lotus::Mesh>(); 
        mesh->has_transparency = true;

        mesh->texture = lotus::Texture::getTexture(d3m->texture_name);

        mesh->vertex_buffer = engine->renderer.memory_manager->GetBuffer(vertices.size(), vertex_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->index_buffer = engine->renderer.memory_manager->GetBuffer(d3m->num_triangles * 3 * sizeof(uint16_t), index_usage_flags, vk::MemoryPropertyFlagBits::eDeviceLocal);
        mesh->setIndexCount(d3m->num_triangles * 3);
        mesh->setVertexCount(d3m->num_triangles * 3);
        mesh->setVertexInputAttributeDescription(D3M::Vertex::getAttributeDescriptions());
        mesh->setVertexInputBindingDescription(D3M::Vertex::getBindingDescriptions());

        model->meshes.push_back(std::move(mesh));

        engine->worker_pool.addWork(std::make_unique<lotus::ParticleModelInitTask>(engine->renderer.getCurrentImage(), model, std::move(vertices), sizeof(D3M::Vertex)));
    }
}
