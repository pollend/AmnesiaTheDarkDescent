// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2008-2021 NVIDIA Corporation. All rights reserved.

// Copyright 2023 Michael Pollind
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "graphics/ForgeHandles.h"
#include "scene/Viewport.h"

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Utilities/Math/MathTypes.h"
#include "FixPreprocessor.h"


namespace hpl::renderer {

class PipelineHBAOPlus {

public:
    static constexpr uint32_t PreparedDepthCount = 16; // 16 1/4th size depth buffers

    struct ViewportData {
        ViewportData() = default;
        ViewportData(const ViewportData&) = delete;
        ViewportData(ViewportData&& buffer)
            : m_preparedDepth(std::move(buffer.m_preparedDepth))
            , m_perFrameDescriptorSet(std::move(buffer.m_perFrameDescriptorSet))
            , m_constDescriptorSet(std::move(buffer.m_constDescriptorSet))
            , m_size(buffer.m_size)
            , m_frustum(buffer.m_frustum) {
        }

        ViewportData& operator=(const ViewportData&) = delete;

        void operator=(ViewportData&& buffer) {
            m_size = buffer.m_size;
            m_preparedDepth = std::move(buffer.m_preparedDepth);
            m_perFrameDescriptorSet = std::move(buffer.m_perFrameDescriptorSet);
            m_constDescriptorSet = std::move(buffer.m_constDescriptorSet);
            m_preparedDepth = std::move(buffer.m_preparedDepth);
            m_frustum =  buffer.m_frustum;
        }

        // 16 depth intermediary buffers
        ForgeTextureHandle m_preparedDepth = {};
        ForgeTextureHandle m_aoQurter = {};
        std::array<ForgeDescriptorSet, ForgeRenderer::SwapChainLength> m_perFrameDescriptorSet = {};
        ForgeDescriptorSet m_constDescriptorSet = {};
        uint2 m_size = uint2(0, 0);
        cFrustum* m_frustum = nullptr;
    };

    struct HBAORootConstant {
        float2 NDCtoViewMul;
        float2 NDCtoViewAdd;

        float tanHalfFOV;
        float zFar;
        float zNear;
        float outputExp;

        int2 viewportDim;
        float2 viewportTexel;

        int2 viewportQuarterDim;
        float2 viewportQuarterTexel;

        float backgroundViewDepth;
        float foregroundViewDepth;

        float pad[2];
    };
    PipelineHBAOPlus();
    void cmdDraw(
        const ForgeRenderer::Frame& frame,
        cFrustum* apFrustum,
        cViewport* viewport,
        Texture* depthBuffer,
        Texture* normalBuffer,
        Texture* outputBuffer);

private:
    ForgeBufferHandle m_constantBuffer;
    UniqueViewportData<ViewportData> m_boundViewportData;
    RootSignature* m_rootSignature;

    ForgeShaderHandle m_shaderDeinterleave;
    ForgePipelineHandle m_pipelineDeinterleave;

    ForgeShaderHandle m_shaderCourseAO;
    ForgePipelineHandle m_pipelineCourseAO;

    ForgeShaderHandle m_shaderReinterleave;
    ForgePipelineHandle m_pipelineReinterleave;

    Sampler* m_pointSampler;
    ForgeBufferHandle m_constBuffer;
    //    void SetAORadiusConstants(const GFSDK_SSAO_Parameters& Params, const GFSDK::SSAO::InputDepthInfo& InputDepth)
//    {
//        const float RadiusInMeters = Max(Params.Radius, EPSILON);
//        const float R = RadiusInMeters * InputDepth.MetersToViewSpaceUnits;
//        m_Data.fR2 = R * R;
//        m_Data.fNegInvR2 = -1.f / m_Data.fR2;
//
//        const float TanHalfFovy = InputDepth.ProjectionMatrixInfo.GetTanHalfFovY();
//        m_Data.fRadiusToScreen = R * 0.5f / TanHalfFovy * InputDepth.Viewport.Height;
//
//        const float BackgroundViewDepth = Max(Params.BackgroundAO.BackgroundViewDepth, EPSILON);
//        m_Data.fBackgroundAORadiusPixels = Params.BackgroundAO.Enable ? (m_Data.fRadiusToScreen / BackgroundViewDepth) : -1.f;
//
//        const float ForegroundViewDepth = Max(Params.ForegroundAO.ForegroundViewDepth, EPSILON);
//        m_Data.fForegroundAORadiusPixels = Params.ForegroundAO.Enable ? (m_Data.fRadiusToScreen / ForegroundViewDepth) : -1.f;
//    }
};

}

