#include <graphics/Pipeline.h>
#include "graphics/Material.h"

namespace hpl {


    //   ForgeTextureHandle::ForgeTextureHandle(HPLPipeline* pipeline):
    //         m_pipeline(pipeline),
    //         m_refCounter(new RefCounter()){
    //         m_refCounter->m_refCount++;
    //     }
    //     ForgeTextureHandle::ForgeTextureHandle(const ForgeTextureHandle& other) {
    //         m_handle = other.m_handle;
    //         m_pipeline = other.m_pipeline;
    //         m_refCounter = other.m_refCounter;
    //         m_refCounter->m_refCount++;
    //     }
    //     ForgeTextureHandle::~ForgeTextureHandle() {
    //         if(m_refCounter && (--m_refCounter->m_refCount) == 0) {
    //             delete m_refCounter;
    //         }
    //     }
    //     ForgeTextureHandle::ForgeTextureHandle(ForgeTextureHandle&& other) {
    //         m_handle = other.m_handle;
    //         m_pipeline = other.m_pipeline;
    //         m_refCounter = other.m_refCounter;
    //         other.m_handle = nullptr;
    //         other.m_pipeline = nullptr;
    //         other.m_refCounter = nullptr;
    //     }
        

    // ForgeTextureHandle::~ForgeTextureHandle() {
    //     if(m_handle) {
    //         removeResource(m_handle);
    //     }
    // }

    // ForgeBufferHandle::~ForgeBufferHandle() {
    //     if(m_handle) {
    //         removeResource(m_handle);
    //     }
    // }

    void ForgeTextureHandle::Free() {
        if(m_handle) {
            removeResource(m_handle);
            // m_pipeline->removeResource(m_handle);
        }
    }

    void ForgeBufferHandle::Free() {
        if(m_handle) {
            removeResource(m_handle);
            // m_pipeline->removeResource(m_handle);
        }
    }

    void ForgeDescriptorSet::Free() {
        if(m_handle) {
            ASSERT(m_renderer && "Renderer is null");
            removeDescriptorSet(m_renderer, m_handle);
        }
    }

    void HPLPipeline::CommandPool::AddMaterial(cMaterial& material, std::span<eMaterialTexture> textures) {
        for(auto& tex: textures) {
            auto image = material.GetImage(tex);
            if(image) {
                // auto texture = image->GetTexture();
                // if(texture) {
                //     // m_cmds.push_back(texture);
                // }
            }
        }
    }

};