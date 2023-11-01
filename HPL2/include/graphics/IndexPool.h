#pragma once

#include "graphics/ForgeHandles.h"
#include <cstdint>

#include <folly/small_vector.h>
#include <set>
#include <span>
#include <vector>

namespace hpl {
    class IndexPool {
    public:
        IndexPool(uint32_t reserve);

        uint32_t requestId();
        void returnId(uint32_t);
    private:
        struct IdRange {
            uint32_t m_start;
            uint32_t m_end;
        };
        folly::small_vector<IdRange, 256> m_avaliable;
    };


    class IndexPoolHandle {
    public:
        explicit inline IndexPoolHandle(): m_pool(nullptr){}
        explicit inline IndexPoolHandle(IndexPool* pool): m_pool(pool) {
            m_index = m_pool->requestId();
        }
        inline ~IndexPoolHandle() {
            if(m_index != UINT32_MAX) {
                m_pool->returnId(m_index);
            }
            m_index = UINT32_MAX;
        }

        explicit operator uint32_t() const {
            return m_index;
        }
        inline bool isValid() {
            return m_index != UINT32_MAX;
        }
        inline uint32_t get() {
            return m_index;
        }
        inline IndexPoolHandle(IndexPoolHandle&& handle) {
            if(m_index != UINT32_MAX) {
                m_pool->returnId(m_index);
            }
            m_index = handle.m_index;
            m_pool = handle.m_pool;
            handle.m_index = UINT32_MAX;
        }
        inline IndexPoolHandle(IndexPoolHandle& handle) = delete;

        inline IndexPoolHandle& operator=(IndexPoolHandle& handle) = delete;
        inline IndexPoolHandle& operator=(IndexPoolHandle&& handle) {
            if (m_index != UINT32_MAX) {
                m_pool->returnId(m_index);
            }
            m_index = handle.m_index;
            m_pool = handle.m_pool;
            handle.m_index = UINT32_MAX;
            return *this;
        }

        inline IndexPool* pool() {
            return m_pool;
        }
    private:
        uint32_t m_index = UINT32_MAX;
        IndexPool* m_pool = nullptr;
    };

    class IndexRingDisposable {
    public:
        struct DisposableResource {
            IndexPoolHandle m_handle;
            uint8_t m_count = 0;

            DisposableResource(IndexPoolHandle&& handler): m_handle(std::move(handler)) {};
            DisposableResource(DisposableResource&&) = default;
            DisposableResource(const DisposableResource&) = delete;
            void operator=(const DisposableResource&) = delete;
            DisposableResource& operator=(DisposableResource&&) = default;
        };

        explicit inline IndexRingDisposable(uint32_t size) {
            m_ring.resize(size);
        }
        void reset(std::function<void(IndexPoolHandle&)> handler) {
            m_index = ((++m_index) % m_ring.size());
            for (auto& resource : m_ring[m_index]) {
                handler(resource.m_handle);
                resource.m_count++;
                if(resource.m_count <= m_ring.size()) {
                    m_ring[(m_index + 1) % m_ring.size()].emplace_back(std::move(resource));
                }
            }
            m_ring[m_index].clear();
        }

        void push(SharedResourceVariant resource, IndexPoolHandle&& handler) {
            m_ring[m_index].emplace_back(DisposableResource(std::move(handler)));
        }

    private:
        uint32_t m_index = 0;
        folly::small_vector<std::vector<DisposableResource>, 4> m_ring;
    };

    template<class T>
    class IndexResourceRingDisposable {
    public:
        struct DisposableResource {
            T m_resource;
            IndexPoolHandle m_handle;
            uint8_t m_count = 0;

            DisposableResource(T&& resource, IndexPoolHandle&& handler): m_resource(std::move(resource)), m_handle(std::move(handler)) {};
            DisposableResource(DisposableResource&&) = default;
            DisposableResource(const DisposableResource&) = delete;
            void operator=(const DisposableResource&) = delete;
            DisposableResource& operator=(DisposableResource&&) = default;
        };

        explicit IndexResourceRingDisposable(uint32_t size) {
            m_ring.resize(size);
        }
        IndexResourceRingDisposable() {
        }

        void reset(std::function<void(T&, IndexPoolHandle&)> resetHandle) {
            m_index = ((++m_index) % m_ring.size());
            for (auto& resource : m_ring[m_index]) {
                resetHandle(resource.m_resource, resource.m_handle);
                resource.m_count++;
                if(resource.m_count <= m_ring.size()) {
                    m_ring[(m_index + 1) % m_ring.size()].emplace_back(std::move(resource));
                }
            }
            m_ring[m_index].clear();
        }

        void push(T&& resource, IndexPoolHandle&& handler) {
            m_ring[m_index].emplace_back(DisposableResource(std::move(resource), std::move(handler)));
        }

    private:
        uint32_t m_index = 0;
        folly::small_vector<std::vector<DisposableResource>, 4> m_ring;
    };
}

