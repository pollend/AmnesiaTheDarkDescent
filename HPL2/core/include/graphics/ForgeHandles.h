#pragma once

#include "graphics/GraphicsTypes.h"

#include <atomic>
#include <functional>
#include <span>
#include <folly/hash/Hash.h>

#include "Common_3/Graphics/Interfaces/IGraphics.h"
#include "Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "FixPreprocessor.h"

namespace hpl {
    class cBitmap;

    // a handle that can be used to reference a resource
    template<class CRTP, class T>
    struct RefHandle {
    public:
        using Base = RefHandle<CRTP, T>;
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

        RefHandle() = default;

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
        [[deprecated("use load easy to forget to call Initialize")]]
        void Initialize() {
            if(!m_initialized && m_handle) {
                ASSERT(!m_refCounter && "Trying to Initialize a handle with references");
                if(!m_refCounter) {
                    m_refCounter = new RefCounter();
                }
                m_refCounter->m_refCount++;
                m_initialized = true;
            }
        }

        void Load(std::function<bool(T** handle)> load) {
            TryFree();
            if(!m_initialized) {
                ASSERT(!m_handle && "Handle is not null");
                if(load(&m_handle)) {
                    m_owning = true;
                    ASSERT(m_handle && "Handle is null");
                    ASSERT(!m_refCounter && "Trying to Initialize a handle with references");
                    if(!m_refCounter) {
                        m_refCounter = new RefCounter();
                    }
                    m_refCounter->m_refCount++;
                    m_initialized = true;
                } else {
                    ASSERT(!m_handle && "Handle is not null");
                }
            }
        }

        void TryFree() {
            // we don't own the handle so we can't free it
            if(!m_initialized) {
                ASSERT(!m_handle && "Handle is not null");
                // we only have a refcounter if we own the handle
                ASSERT(((!m_refCounter && m_owning) || !m_owning) && "RefCounter is not null");
                return;
            }
            if(!m_owning) {
                ASSERT(!m_refCounter && "RefCounter is not null"); // we should have a refcounter if we don't own the handle
                m_handle = nullptr;
                m_initialized = false;
                return;
            }

            ASSERT(m_initialized && "Trying to free a handle that has not been initialized");
            ASSERT(m_refCounter && "Trying to free a handle that has not been initialized");
            ASSERT(m_refCounter->m_refCount > 0 && "Trying to free resource that is still referenced");
            if((--m_refCounter->m_refCount) == 0) {
                static_cast<CRTP*>(this)->Free(); // Free the underlying resource
                delete m_refCounter;
            }
            m_handle = nullptr;
            m_initialized = false;
            m_refCounter = nullptr;
        }

        void operator= (const RefHandle& other) {
            TryFree(); // Free the current handle
            m_handle = other.m_handle;
            m_refCounter = other.m_refCounter;
            m_initialized = other.m_initialized;
            m_owning = other.m_owning;
            if(m_initialized && m_owning) {
                ASSERT(m_handle && "Handle is null");
                ASSERT(m_refCounter && "RefCounter is null");
                m_refCounter->m_refCount++;
            }
        }

        void operator= (RefHandle&& other) {
            TryFree(); // Free the current handle
            m_handle = other.m_handle;
            m_refCounter = other.m_refCounter;
            m_initialized = other.m_initialized;
            m_owning = other.m_owning;
            other.m_owning = false;
            other.m_handle = nullptr;
            other.m_refCounter = nullptr;
            other.m_initialized = false;
        }

        bool IsValid() const {
            return m_handle != nullptr;
        }

    protected:
        bool m_owning = true;
        bool m_initialized = false;
        RefCounter* m_refCounter = nullptr;
        friend CRTP;
    };

    struct ForgeRenderTarget : public RefHandle<ForgeRenderTarget, RenderTarget> {
    public:
        using Base = RefHandle<ForgeRenderTarget, RenderTarget>;
        ForgeRenderTarget()
            : Base() {
        }
        ForgeRenderTarget(Renderer* renderer)
            : m_renderer(renderer)
            , Base() {
        }
        ForgeRenderTarget(const ForgeRenderTarget& other)
            : Base(other)
            , m_renderer(other.m_renderer) {
        }
        ForgeRenderTarget(ForgeRenderTarget&& other)
            : Base(std::move(other))
            , m_renderer(other.m_renderer) {
        }
        ~ForgeRenderTarget() {
        }
        void operator=(const ForgeRenderTarget& other) {
            Base::operator=(other);
            m_renderer = other.m_renderer;
        }
        void operator=(ForgeRenderTarget&& other) {
            Base::operator=(std::move(other));
            m_renderer = other.m_renderer;
        }

        void Load(Renderer* renderer, std::function<bool(RenderTarget** handle)> load) {
            ASSERT(renderer && "Renderer is null");
            m_renderer = renderer;
            static_cast<Base*>(this)->Load(load);
        }
    private:
        void Free();
        Renderer* m_renderer = nullptr;
        friend class RefHandle<ForgeRenderTarget, RenderTarget>;
    };

    struct ForgeTextureHandle: public RefHandle<ForgeTextureHandle, Texture> {
    public:
        struct BitmapLoadOptions {
        public:
            bool m_useCubeMap = false;
            bool m_useArray = false;
            bool m_useMipmaps = false;
        };

        struct BitmapCubmapLoadOptions {
            bool m_useMipmaps: 1;
        };

        static ForgeTextureHandle LoadFromHPLBitmap(cBitmap& bitmap, const BitmapLoadOptions& options);
        static ForgeTextureHandle CreateCubemapFromHPLBitmaps(const std::span<cBitmap*> bitmaps, const BitmapCubmapLoadOptions& options);
        static TinyImageFormat FromHPLPixelFormat(ePixelFormat format);

        ForgeTextureHandle():
            Base() {
        }
        ForgeTextureHandle(const ForgeTextureHandle& other):
            Base(other), m_renderTarget(other.m_renderTarget){
        }
        ForgeTextureHandle(ForgeTextureHandle&& other):
            Base(std::move(other)),
            m_renderTarget(std::move(other.m_renderTarget)) {
        }
        ~ForgeTextureHandle() {
        }

        void operator= (const ForgeTextureHandle& other) {
            Base::operator=(other);
            m_renderTarget = other.m_renderTarget;
        }

        void operator= (ForgeTextureHandle&& other) {
            Base::operator=(std::move(other));
            m_renderTarget = std::move(other.m_renderTarget);
        }

        void SetRenderTarget(ForgeRenderTarget renderTarget) {
            TryFree();
            m_initialized = true;
            m_owning = false; // We don't own the handle we are attaching
            m_handle = renderTarget.m_handle->pTexture;
            m_renderTarget = renderTarget; // We don't own the handle
        }
    private:
        void Free();
        ForgeRenderTarget m_renderTarget{};
        friend class RefHandle<ForgeTextureHandle, Texture>;
    };

    struct ForgeDescriptorSet final: public RefHandle<ForgeDescriptorSet, DescriptorSet> {
    public:
        ForgeDescriptorSet():
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
        void Load(Renderer* renderer, std::function<bool(DescriptorSet** handle)> load) {
            ASSERT(renderer && "Renderer is null");
            m_renderer = renderer;
            Base::Load(load);
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

    struct ForgePipelineHandle: public RefHandle<ForgePipelineHandle, Pipeline> {
    public:
        ForgePipelineHandle():
            Base() {
        }
        ForgePipelineHandle(const ForgePipelineHandle& other):
            Base(other),
            m_renderer(other.m_renderer) {
        }
        ForgePipelineHandle(ForgePipelineHandle&& other):
            Base(std::move(other)){
        }
        ~ForgePipelineHandle() {
        }

        void operator= (const ForgePipelineHandle& other) {
            Base::operator=(other);
            m_renderer = other.m_renderer;
        }
        void operator= (ForgePipelineHandle&& other) {
            Base::operator=(std::move(other));
            m_renderer = other.m_renderer;
        }

        void Load(Renderer* renderer, std::function<bool(Pipeline** handle)> load) {
            ASSERT(renderer && "Renderer is null");
            m_renderer = renderer;
            Base::Load(load);
        }
    private:
        void Free();
        Renderer* m_renderer = nullptr;
        friend class RefHandle<ForgePipelineHandle, Pipeline>;
    };

    struct ForgeShaderHandle: public RefHandle<ForgeShaderHandle, Shader> {
    public:
        ForgeShaderHandle():
            Base() {
        }
        ForgeShaderHandle(const ForgeShaderHandle& other):
            Base(other),
            m_renderer(other.m_renderer) {
        }
        ForgeShaderHandle(ForgeShaderHandle&& other):
            Base(std::move(other)){
        }
        ~ForgeShaderHandle() {
        }

        void operator= (const ForgeShaderHandle& other) {
            Base::operator=(other);
            m_renderer = other.m_renderer;
        }
        void operator= (ForgeShaderHandle&& other) {
            Base::operator=(std::move(other));
            m_renderer = other.m_renderer;
        }

        void Load(Renderer* renderer, std::function<bool(Shader** handle)> load) {
            ASSERT(renderer && "Renderer is null");
            m_renderer = renderer;
            Base::Load(load);
        }
    private:
        void Free();
        Renderer* m_renderer = nullptr;
        friend class RefHandle<ForgeShaderHandle, Shader>;
    };
    struct ForgeSamplerHandle: public RefHandle<ForgeSamplerHandle, Sampler> {
    public:
        ForgeSamplerHandle():
            Base() {
        }
        ForgeSamplerHandle(const ForgeSamplerHandle& other):
            Base(other) {
            m_renderer = other.m_renderer;
        }
        ForgeSamplerHandle(ForgeSamplerHandle&& other):
            Base(std::move(other)),
            m_renderer(other.m_renderer) {

        }
        ~ForgeSamplerHandle() {
        }

        void Load(Renderer* renderer, std::function<bool(Sampler** handle)> load) {
            ASSERT(renderer && "Renderer is null");
            m_renderer = renderer;
            Base::Load(load);
        }

        void operator= (const ForgeSamplerHandle& other) {
            Base::operator=(other);
            m_renderer = other.m_renderer;
        }
        void operator= (ForgeSamplerHandle&& other) {
            Base::operator=(std::move(other));
            m_renderer = other.m_renderer;
        }
    private:
        void Free();
        Renderer* m_renderer = nullptr;
        friend class RefHandle<ForgeSamplerHandle, Sampler>;
    };

    struct ForgeSwapChainHandle: public RefHandle<ForgeSwapChainHandle, SwapChain> {

        ForgeSwapChainHandle():
            Base() {
        }
        ForgeSwapChainHandle(const ForgeSwapChainHandle& other):
            Base(other) {
            m_renderer = other.m_renderer;
        }
        ForgeSwapChainHandle(ForgeSwapChainHandle&& other):
            Base(std::move(other)),
            m_renderer(other.m_renderer) {

        }
        ~ForgeSwapChainHandle() {
        }

        void Load(Renderer* renderer, std::function<bool(SwapChain** handle)> load) {
            ASSERT(renderer && "Renderer is null");
            m_renderer = renderer;
            Base::Load(load);
        }

        void operator= (const ForgeSwapChainHandle& other) {
            Base::operator=(other);
            m_renderer = other.m_renderer;
        }
        void operator= (ForgeSwapChainHandle&& other) {
            Base::operator=(std::move(other));
            m_renderer = other.m_renderer;
        }
    private:
        void Free();
        Renderer* m_renderer = nullptr;
        friend class RefHandle<ForgeSwapChainHandle, SwapChain>;
    };

    struct ForgeCmdHandle: public RefHandle<ForgeCmdHandle, Cmd> {
    public:
        ForgeCmdHandle(Renderer* renderer):
            Base(),
            m_renderer(renderer) {
        }
        ForgeCmdHandle(const ForgeCmdHandle& other):
            Base(other) {
            m_renderer = other.m_renderer;
        }
        ForgeCmdHandle(ForgeCmdHandle&& other):
            Base(std::move(other)),
            m_renderer(other.m_renderer) {

        }
        ~ForgeCmdHandle() {
        }

        void Load(Renderer* renderer, std::function<bool(Cmd** handle)> load) {
            ASSERT(renderer && "Renderer is null");
            m_renderer = renderer;
            Base::Load(load);
        }
        void operator= (const ForgeCmdHandle& other) {
            Base::operator=(other);
            m_renderer = other.m_renderer;
        }
        void operator= (ForgeCmdHandle&& other) {
            Base::operator=(std::move(other));
            m_renderer = other.m_renderer;
        }
    private:
        void Free();
        Renderer* m_renderer = nullptr;
        friend class RefHandle<ForgeCmdHandle, Cmd>;
    };

}

static bool operator==(const SamplerDesc& lhs, const SamplerDesc& rhs) {
   return rhs.mMinFilter  ==   lhs.mMinFilter &&
       rhs.mMagFilter  ==   lhs.mMagFilter &&
       rhs.mMipMapMode ==   lhs.mMipMapMode &&
       rhs.mAddressU   ==   lhs.mAddressU &&
       rhs.mAddressV   ==   lhs.mAddressV &&
       rhs.mAddressW   ==   lhs.mAddressW &&
       rhs.mMipLodBias ==   lhs.mMipLodBias &&
       rhs.mSetLodRange==   lhs.mSetLodRange &&
       rhs.mMinLod     ==   lhs.mMinLod &&
       rhs.mMaxLod     ==   lhs.mMaxLod &&
       rhs.mMaxAnisotropy== lhs.mMaxAnisotropy &&
       rhs.mCompareFunc  == lhs.mCompareFunc;
}

namespace std {
    template<>
    struct hash<SamplerDesc> {
        size_t operator()(const SamplerDesc& desc) const {
            return folly::hash::fnv32_buf(&desc, sizeof(desc));
        }
    };
}

