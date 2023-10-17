/// Copyright 2023 Michael Pollind
/// SPDX-License-Identifier: GPL-3.0

#include "graphics/SubMeshResource.h"

#include "resources/MaterialManager.h"

namespace hpl {

    SubMeshResource::SubMeshResource(const tString& asName, cMaterialManager* apMaterialManager)
        : m_materialManager(apMaterialManager)
        , m_name(asName) {
    }
    SubMeshResource::SubMeshResource(SubMeshResource&& resource)
        : m_materialManager(resource.m_materialManager)
        , m_name(std::move(resource.m_name))
        , m_materialName(std::move(resource.m_materialName))
        , m_mtxLocalTransform(resource.m_mtxLocalTransform)
        , m_vtxBonePairs(std::move(resource.m_vtxBonePairs))
        , m_colliders(std::move(resource.m_colliders))
        , m_vertexStreams(std::move(resource.m_vertexStreams))
        , m_index(std::move(resource.m_index))
        , m_vertexWeights(std::move(resource.m_vertexWeights))
        , m_vertexBones(std::move(resource.m_vertexBones))
        , m_modelScale(resource.m_modelScale)
        , m_doubleSided(resource.m_doubleSided)
        , m_collideShape(resource.m_collideShape)
        , m_isOneSided(resource.m_isOneSided)
        , m_oneSidedNormal(resource.m_oneSidedNormal)
        , m_oneSidedPoint(resource.m_oneSidedPoint) {
        resource.m_material = nullptr;

    }

    SubMeshResource::~SubMeshResource() {
        if (m_material) {
            m_materialManager->Destroy(m_material);
        }
    }
    void SubMeshResource::operator=(SubMeshResource&& resource) {
        m_materialManager = resource.m_materialManager;
        m_name = std::move(resource.m_name);
        m_materialName = std::move(resource.m_materialName);
        m_mtxLocalTransform = resource.m_mtxLocalTransform;
        m_vtxBonePairs = std::move(resource.m_vtxBonePairs);
        m_colliders = std::move(resource.m_colliders);
        m_vertexStreams = std::move(resource.m_vertexStreams);
        m_index = std::move(resource.m_index);
        m_vertexWeights = std::move(resource.m_vertexWeights);
        m_vertexBones = std::move(resource.m_vertexBones);
        m_modelScale = resource.m_modelScale;
        m_doubleSided = resource.m_doubleSided;
        m_collideShape = resource.m_collideShape;
        m_isOneSided = resource.m_isOneSided;
        m_oneSidedNormal = resource.m_oneSidedNormal;
        m_oneSidedPoint = resource.m_oneSidedPoint;
        resource.m_material = nullptr;
    }

    void SubMeshResource::SetStreamBuffer(const std::vector<StreamBufferInfo>& info) {
        m_vertexStreams = info;
    }
    void SubMeshResource::SetStreamBuffer(std::vector<StreamBufferInfo>&& info) {
        m_vertexStreams = std::move(info);
    }

    void SubMeshResource::SetMaterial(cMaterial* apMaterial) {
        if (m_material)
            m_materialManager->Destroy(m_material);
        m_material = apMaterial;
    }

    void SubMeshResource::SetIndexBuffer(IndexBufferInfo&& info) {
        m_index = std::move(info);
    }
    void SubMeshResource::SetIndexBuffer(IndexBufferInfo& info) {
        m_index = info;
    }
    std::span<SubMeshResource::StreamBufferInfo> SubMeshResource::GetStreamBuffers() {
        return m_vertexStreams;
    }
    std::span<cVertexBonePair> SubMeshResource::GetVertexBonePairs() {
        return m_vtxBonePairs;
    }

    void SubMeshResource::SetVertexBonePairs(const std::vector<cVertexBonePair>& pair) {
        m_vtxBonePairs = pair;
    }

    void SubMeshResource::SetVertexBonePairs(const std::vector<cVertexBonePair>&& pair) {
        m_vtxBonePairs = std::move(pair);
    }
    void SubMeshResource::SetCollisionResource(const std::vector<MeshCollisionResource>& info) {
        m_colliders = info;
    }
    void SubMeshResource::SetCollisionResource(std::vector<MeshCollisionResource>&& info) {
        m_colliders = std::move(info);
    }
} // namespace hpl
