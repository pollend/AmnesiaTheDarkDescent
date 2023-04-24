#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <queue>
#include <span>
#include <variant>
#include <vector>

#include "absl/container/inlined_vector.h"

#include "graphics/GraphicsTypes.h"

#include <engine/RTTI.h>

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "FixPreprocessor.h"

namespace hpl {
    class cMaterial;
    class HPLPipeline;

    // a handle that can be used to reference a resource
    template<class TBase, class T>
    struct RefHandle {
    public:
        using Base = RefHandle<TBase, T>;
        // NOTE: This is a pointer to the resource 
        //      Just how the API works for the-forget this is exposed to the user so be careful
        T* m_handle = nullptr; 

        struct RefCounter {
        public:
            RefCounter() = default;
            RefCounter(const RefCounter&) = delete;
            RefCounter& operator=(const RefCounter&) = delete;
            RefCounter(RefCounter&&) = delete;
            RefCounter& operator=(RefCounter&&) = delete;
            std::atomic<uint32_t> m_refCount = 0;
        };

        RefHandle() {
        }

        RefHandle(const RefHandle& other) {
            m_handle = other.m_handle;
            m_refCounter = other.m_refCounter;
            m_initialized = other.m_initialized;
            if(m_initialized) {
                ASSERT(m_refCounter && "RefCounter is null");
                m_refCounter->m_refCount++;
            }
        }

        ~RefHandle() {
            TryFree();
            if(m_refCounter) {
                ASSERT(m_refCounter->m_refCount == 0 && "Trying to free a handle that still has references");
                delete m_refCounter;
            }
        }

        RefHandle(RefHandle&& other) {
            m_handle = other.m_handle;
            m_refCounter = other.m_refCounter;
            m_initialized = other.m_initialized;
            other.m_handle = nullptr;
            other.m_refCounter = nullptr;
            other.m_initialized = false;
        }

        /**
        * Initialize the handle with a resource
        */
        void Initialize() {
            if(!m_initialized && m_handle) {
                ASSERT(m_refCounter && m_refCounter->m_refCount > 0 && "Trying to Initialize a handle with references");
                if(!m_refCounter) {
                    m_refCounter = new RefCounter();
                }
                m_refCounter->m_refCount++;
                m_initialized = true;
            }
        }

        void TryFree() {
            if(!m_handle) {
                return;
            }
            ASSERT(m_refCounter && "Trying to free a handle that has not been initialized");
            ASSERT(m_refCounter->m_refCount == 0 && "Trying to free resource that is still referenced");
            if((--m_refCounter->m_refCount) == 0) {
                static_cast<TBase*>(this)->Free(); // Free the underlying resource
            }
            m_handle = nullptr;
            m_initialized = false;
        }
        
        void operator= (const RefHandle& other) {
            m_handle = other.m_handle;
            m_refCounter = other.m_refCounter;
            m_initialized = other.m_initialized;
            if(m_initialized) {
                ASSERT(m_refCounter && "RefCounter is null");
                m_refCounter->m_refCount++;
            }
        }
        
        void operator= (RefHandle&& other) {
            m_handle = other.m_handle;
            m_refCounter = other.m_refCounter;
            other.m_handle = nullptr;
            other.m_refCounter = nullptr;
        }

        bool IsValid() {
            return m_handle != nullptr;
        }
        
    protected:
        bool m_initialized = false;
        RefCounter* m_refCounter = nullptr;
        friend TBase;
    };


    struct ForgeTextureHandle: public RefHandle<ForgeTextureHandle, Texture> {
    public:
        ForgeTextureHandle():
            Base() {
        }
        ForgeTextureHandle(const ForgeTextureHandle& other):
            Base(other) {
        }
        ForgeTextureHandle(ForgeTextureHandle&& other):
            Base(std::move(other)) {
        }
        ~ForgeTextureHandle() {
        }
        void operator= (const ForgeTextureHandle& other) {
            Base::operator=(other);
        }
        void operator= (ForgeTextureHandle&& other) {
            Base::operator=(std::move(other));
        }

    private:
        void Free();
        friend class RefHandle<ForgeTextureHandle, Texture>;
    };


    struct ForgeDescriptorSet: public RefHandle<ForgeDescriptorSet, DescriptorSet> {
    public:
        ForgeDescriptorSet():
            Base() {
        }
        ForgeDescriptorSet(Renderer* renderer):
            m_renderer(renderer),
            Base() {
        }
        ForgeDescriptorSet(const ForgeDescriptorSet& other):
            Base(other),
            m_renderer(other.m_renderer) {

        }
        ForgeDescriptorSet(ForgeDescriptorSet&& other):
            Base(std::move(other)),
            m_renderer(other.m_renderer){
        }
        ~ForgeDescriptorSet() {
        }
        void operator= (const ForgeDescriptorSet& other) {
            Base::operator=(other);
            m_renderer = other.m_renderer;
        }
        void operator= (ForgeDescriptorSet&& other) {
            Base::operator=(std::move(other));
            m_renderer = other.m_renderer;
        }

    private:
        void Free();
        Renderer* m_renderer = nullptr;
        friend class RefHandle<ForgeDescriptorSet, DescriptorSet>;
    };

    struct ForgeBufferHandle : public RefHandle<ForgeBufferHandle, Buffer> {
    public:
        ForgeBufferHandle():
            Base() {
        }
        ForgeBufferHandle(const ForgeBufferHandle& other):
            Base(other) {
        }
        ForgeBufferHandle(ForgeBufferHandle&& other):
            Base(std::move(other)) {
        }
        ~ForgeBufferHandle() {
        }

        void operator= (const ForgeBufferHandle& other) {
            Base::operator=(other);
        }
        void operator= (ForgeBufferHandle&& other) {
            Base::operator=(std::move(other));
        }
    private:
        void Free();
        friend class RefHandle<ForgeBufferHandle, Buffer>;
    };


    class HPLPipeline final {
        HPL_RTTI_CLASS(HPLPipeline, "{66526c65-ad10-4a59-af06-103f34d1cb57}")
    public:
        HPLPipeline() = default;

        static constexpr uint32_t SwapChainLength = 2; // double buffered
		static constexpr uint32_t MaxMaterialHandles = 16384;

        class CommandPool {
        public:
            CommandPool() = default;

            void AddMaterial(cMaterial& material, std::span<eMaterialTexture> textures);
            void ResetPool();
        private:
            absl::InlinedVector<std::variant<ForgeTextureHandle, ForgeBufferHandle>, 1024> m_cmds;
        };

        // increment frame index and swap chain index
        void IncrementFrame() {
            acquireNextImage(m_renderer, m_swapChain, m_imageAcquiredSemaphore, nullptr, &m_swapChainIndex);
        }
        
        RootSignature* Root() { return m_rootSignature; }
        Renderer* Rend() { return m_renderer; }
        
        size_t SwapChainIndex() { return m_swapChainIndex; }
        size_t CurrentFrame() { return m_currentFrameIndex; }
        size_t FrameIndex()  { return (m_currentFrameIndex % SwapChainLength); }
        CommandPool& CmdPool(size_t index) { return m_commandPool[index]; }
        
    private:
        std::array<CommandPool, SwapChainLength> m_commandPool;
        Renderer* m_renderer;
        RootSignature* m_rootSignature;
        SwapChain* m_swapChain;
        Semaphore* m_imageAcquiredSemaphore;

        uint32_t m_currentFrameIndex = 0;
        uint32_t m_swapChainIndex = 0;
    };
}