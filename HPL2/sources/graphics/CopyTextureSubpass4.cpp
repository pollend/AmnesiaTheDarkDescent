#include "graphics/CopyTextureSubpass4.h"

namespace hpl {

    CopyTextureSubpass4::CopyTextureSubpass4(hpl::ForgeRenderer& renderer) {

        m_copyShader.Load(renderer.Rend(), [&](Shader** shader) {
            ShaderLoadDesc loadDesc = {};
            loadDesc.mStages[0].pFileName = "copy_channel_4.comp";
            addShader(renderer.Rend(), &loadDesc, shader);
            return true;
        });
        m_copyRootSignature.Load(renderer.Rend(), [&](RootSignature** signature) {
            RootSignatureDesc refractionCopyRootDesc = {};
            refractionCopyRootDesc.mShaderCount = 1;
            refractionCopyRootDesc.ppShaders = &m_copyShader.m_handle;
            addRootSignature(renderer.Rend(), &refractionCopyRootDesc, signature);
            return true;
        });
        m_copyPipline.Load(renderer.Rend(), [&](Pipeline** pipeline) {
            PipelineDesc desc = {};
            desc.mType = PIPELINE_TYPE_COMPUTE;
            ComputePipelineDesc& pipelineSettings = desc.mComputeDesc;
            pipelineSettings.pShaderProgram = m_copyShader.m_handle;
            pipelineSettings.pRootSignature = m_copyRootSignature.m_handle;
            addPipeline(renderer.Rend(), &desc, pipeline);
            return true;
        });
        DescriptorSetDesc setDesc = { m_copyRootSignature.m_handle,
                                      DESCRIPTOR_UPDATE_FREQ_PER_FRAME,
                                      CopyTextureSubpass4::NumberOfDescriptors };
        for (auto& frameset : m_copyPerFrameSet) {
            frameset.Load(renderer.Rend(), [&](DescriptorSet** descriptor) {
                addDescriptorSet(renderer.Rend(), &setDesc, descriptor);
                return true;
            });
        }
        for(auto& idx: m_index) {
            idx = 0;
        }
    }

    void CopyTextureSubpass4::Dispatch(ForgeRenderer::Frame& frame, Texture* src, Texture* dest) {
            std::array params = {
                DescriptorData {.pName = "sourceInput", .ppTextures = &src},
                DescriptorData {.pName = "destOutput", .ppTextures = &dest}
            };
            m_index[frame.index()] = (m_index[frame.index()] + 1) % NumberOfDescriptors;
            updateDescriptorSet(
                frame.m_renderer->Rend(), m_index[frame.index()], m_copyPerFrameSet[frame.m_frameIndex].m_handle, params.size(), params.data());
            cmdBindPipeline(
                frame.m_cmd, m_copyPipline.m_handle);
            cmdBindDescriptorSet(frame.m_cmd, m_index[frame.index()], m_copyPerFrameSet[frame.m_frameIndex].m_handle);
            cmdDispatch(
                frame.m_cmd,
                static_cast<uint32_t>(static_cast<float>(dest->mWidth) / 16.0f) + 1,
                static_cast<uint32_t>(static_cast<float>(dest->mHeight) / 16.0f) + 1,
                1);
    }
}
