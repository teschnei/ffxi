#pragma once
#include "../work_item.h"
#include "../entity/renderable_entity.h"
#include <engine/renderer/vulkan/vulkan_inc.h>

namespace lotus
{
    class Particle;
    class ParticleEntityInitTask : public WorkItem
    {
    public:
        ParticleEntityInitTask(const std::shared_ptr<Particle>& entity);
        virtual void Process(WorkerThread*) override;
    protected:
        void createStaticCommandBuffers(WorkerThread* thread);
        void drawModel(WorkerThread* thread, vk::CommandBuffer buffer, bool transparency, vk::PipelineLayout, size_t image);
        void drawMesh(WorkerThread* thread, vk::CommandBuffer buffer, const Mesh& mesh, vk::PipelineLayout, uint32_t material_index);
        std::shared_ptr<Particle> entity;
        std::vector<std::unique_ptr<Buffer>> staging_buffers;
        vk::UniqueHandle<vk::CommandBuffer, vk::DispatchLoaderDynamic> command_buffer;
    };


    class ParticleEntityReInitTask : public ParticleEntityInitTask
    {
    public:
        ParticleEntityReInitTask(const std::shared_ptr<Particle>& entity);
        virtual void Process(WorkerThread*) override;
    };

}
