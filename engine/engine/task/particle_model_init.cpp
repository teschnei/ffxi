#include "particle_model_init.h"
#include <utility>
#include <numeric>
#include "../worker_thread.h"
#include "../core.h"

namespace lotus
{
    ParticleModelInitTask::ParticleModelInitTask(int _image_index, std::shared_ptr<Model> _model, std::vector<uint8_t>&& _vertex_buffer, uint32_t _vertex_stride) :
        WorkItem(), image_index(_image_index), model(std::move(_model)), vertex_buffer(std::move(_vertex_buffer)), vertex_stride(_vertex_stride)
    {
        priority = -1;
    }

    void ParticleModelInitTask::Process(WorkerThread* thread)
    {
        if (!vertex_buffer.empty())
        {
            std::vector<vk::GeometryNV> raytrace_geometry;
            vk::CommandBufferAllocateInfo alloc_info = {};
            alloc_info.level = vk::CommandBufferLevel::ePrimary;
            alloc_info.commandPool = *thread->graphics_pool;
            alloc_info.commandBufferCount = 1;

            auto command_buffers = thread->engine->renderer.device->allocateCommandBuffersUnique<std::allocator<vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic>>>(alloc_info, thread->engine->renderer.dispatch);

            auto& mesh = model->meshes[0];

            auto index_buffer = std::vector<uint16_t>(mesh->getIndexCount());
            std::iota(index_buffer.begin(), index_buffer.end(), 0);

            vk::DeviceSize staging_buffer_size = vertex_buffer.size() + (index_buffer.size() * sizeof(uint16_t));

            staging_buffer = thread->engine->renderer.memory_manager->GetBuffer(staging_buffer_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
            uint8_t* staging_buffer_data = static_cast<uint8_t*>(staging_buffer->map(0, staging_buffer_size, {}));

            command_buffer = std::move(command_buffers[0]);

            vk::CommandBufferBeginInfo begin_info = {};
            begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

            command_buffer->begin(begin_info, thread->engine->renderer.dispatch);

            memcpy(staging_buffer_data, vertex_buffer.data(), vertex_buffer.size());
            memcpy(staging_buffer_data + vertex_buffer.size(), index_buffer.data(), index_buffer.size() * sizeof(uint16_t));

            vk::BufferCopy copy_region;
            copy_region.srcOffset = 0;
            copy_region.size = vertex_buffer.size();
            command_buffer->copyBuffer(staging_buffer->buffer, mesh->vertex_buffer->buffer, copy_region, thread->engine->renderer.dispatch);
            copy_region.size = index_buffer.size() * sizeof(uint16_t);
            copy_region.srcOffset = vertex_buffer.size();
            command_buffer->copyBuffer(staging_buffer->buffer, mesh->index_buffer->buffer, copy_region, thread->engine->renderer.dispatch);

            if (thread->engine->renderer.RTXEnabled())
            {
                auto& geo = raytrace_geometry.emplace_back();
                geo.geometryType = vk::GeometryTypeNV::eTriangles;
                geo.geometry.triangles.vertexData = mesh->vertex_buffer->buffer;
                geo.geometry.triangles.vertexOffset = 0;
                geo.geometry.triangles.vertexCount = static_cast<uint32_t>(vertex_buffer.size() / vertex_stride);
                geo.geometry.triangles.vertexStride = vertex_stride;
                geo.geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;

                geo.geometry.triangles.indexData = mesh->index_buffer->buffer;
                geo.geometry.triangles.indexOffset = 0;
                geo.geometry.triangles.indexCount = static_cast<uint32_t>(index_buffer.size());
                geo.geometry.triangles.indexType = vk::IndexType::eUint16;

                if (!mesh->has_transparency)
                {
                    geo.flags = vk::GeometryFlagBitsNV::eOpaque;
                }
            }

            staging_buffer->unmap();

            if (thread->engine->renderer.RTXEnabled())
            {
                vk::MemoryBarrier barrier;
                barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
                barrier.dstAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteNV | vk::AccessFlagBits::eAccelerationStructureReadNV;
                command_buffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, {}, barrier, nullptr, nullptr, thread->engine->renderer.dispatch);
                model->bottom_level_as = std::make_unique<BottomLevelAccelerationStructure>(thread->engine, *command_buffer, raytrace_geometry, false, model->lifetime == Lifetime::Long, BottomLevelAccelerationStructure::Performance::FastTrace);

                if (model->lifetime == Lifetime::Long && model->rendered)
                {
                    std::vector<vk::DescriptorBufferInfo> descriptor_vertex_info;
                    std::vector<vk::DescriptorBufferInfo> descriptor_index_info;
                    std::vector<vk::DescriptorImageInfo> descriptor_texture_info;
                    //TODO: move these into some kind of thread-safe implementation
                    std::lock_guard lg{ thread->engine->renderer.acceleration_binding_mutex };
                    uint16_t index = thread->engine->renderer.static_acceleration_bindings_offset;
                    for (size_t i = 0; i < model->meshes.size(); ++i)
                    {
                        auto& mesh = model->meshes[i];
                        descriptor_vertex_info.emplace_back(mesh->vertex_buffer->buffer, 0, VK_WHOLE_SIZE);
                        descriptor_index_info.emplace_back(mesh->index_buffer->buffer, 0, VK_WHOLE_SIZE);
                        descriptor_texture_info.emplace_back(*mesh->texture->sampler, *mesh->texture->image_view, vk::ImageLayout::eShaderReadOnlyOptimal);
                        for (int image = 0; image < thread->engine->renderer.getImageCount(); ++image)
                        {
                            thread->engine->renderer.mesh_info_buffer_mapped[image * Renderer::max_acceleration_binding_index + index + i] = { index + (uint32_t)i, index + (uint32_t)i, mesh->specular_exponent, mesh->specular_intensity, model->light_offset };
                        }
                    }
                    model->bottom_level_as->resource_index = index;

                    thread->engine->renderer.static_acceleration_bindings_offset = index + model->meshes.size();
                    vk::WriteDescriptorSet write_info_vertex;
                    write_info_vertex.descriptorType = vk::DescriptorType::eStorageBuffer;
                    write_info_vertex.dstArrayElement = index;
                    write_info_vertex.dstBinding = 1;
                    write_info_vertex.descriptorCount = static_cast<uint32_t>(descriptor_vertex_info.size());
                    write_info_vertex.pBufferInfo = descriptor_vertex_info.data();

                    vk::WriteDescriptorSet write_info_index;
                    write_info_index.descriptorType = vk::DescriptorType::eStorageBuffer;
                    write_info_index.dstArrayElement = index;
                    write_info_index.dstBinding = 2;
                    write_info_index.descriptorCount = static_cast<uint32_t>(descriptor_index_info.size());
                    write_info_index.pBufferInfo = descriptor_index_info.data();

                    vk::WriteDescriptorSet write_info_texture;
                    write_info_texture.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                    write_info_texture.dstArrayElement = index;
                    write_info_texture.dstBinding = 3;
                    write_info_texture.descriptorCount = static_cast<uint32_t>(descriptor_texture_info.size());
                    write_info_texture.pImageInfo = descriptor_texture_info.data();

                    std::vector<vk::WriteDescriptorSet> writes;
                    for (size_t i = 0; i < thread->engine->renderer.getImageCount(); ++i)
                    {
                        write_info_vertex.dstSet = *thread->engine->renderer.rtx_descriptor_sets_const[i];
                        write_info_index.dstSet = *thread->engine->renderer.rtx_descriptor_sets_const[i];
                        write_info_texture.dstSet = *thread->engine->renderer.rtx_descriptor_sets_const[i];
                        writes.push_back(write_info_vertex);
                        writes.push_back(write_info_index);
                        writes.push_back(write_info_texture);
                    }
                    thread->engine->renderer.device->updateDescriptorSets(writes, nullptr, thread->engine->renderer.dispatch);
                }
            }
            command_buffer->end(thread->engine->renderer.dispatch);

            graphics.primary = *command_buffer;
        }
    }
}