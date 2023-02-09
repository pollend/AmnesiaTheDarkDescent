#pragma once

#include <memory>

namespace hpl {

    class HandleWrapper {
        public:
            using Ptr = std::unique_ptr<void, void(*)(void*)>;
            HandleWrapper(): m_ptr(nullptr, [](void*){}) {
            }
            HandleWrapper(Ptr&& ptr)
                : m_ptr(std::move(ptr)) {
            }
            HandleWrapper(HandleWrapper&& other): 
                m_ptr(std::move(other.m_ptr)) {
            }
            HandleWrapper(const HandleWrapper& other) = delete;
            void operator=(HandleWrapper& other) = delete;
            void operator=(HandleWrapper&& other) {
                m_ptr = std::move(other.m_ptr);
            }
            operator bool() const {
                return m_ptr != nullptr;
            }
            
            void* Get() {
                return m_ptr.get();
            }
        private:
            Ptr m_ptr; // debating whether to use shared_ptr or unique_ptr
    };

    class HandleSharedWrapper {
        public:
            using Ptr = std::shared_ptr<void>;
            
            HandleSharedWrapper(): m_ptr(nullptr, [](void*){}) {
            }
            HandleSharedWrapper(Ptr&& ptr)
                : m_ptr(std::move(ptr)) {
            }
            HandleSharedWrapper(HandleSharedWrapper&& other): 
                m_ptr(std::move(other.m_ptr)) {
            }
            HandleSharedWrapper(const HandleSharedWrapper& other):
                m_ptr(other.m_ptr){
            };
            void operator=(HandleSharedWrapper& other) {
                m_ptr = other.m_ptr;
            }
            void operator=(HandleSharedWrapper&& other) {
                m_ptr = std::move(other.m_ptr);
            }
            operator bool() const {
                return m_ptr != nullptr;
            }

            void* Get() {
                return m_ptr.get();
            }
        private:
            std::shared_ptr<void> m_ptr; // debating whether to use shared_ptr or unique_ptr
    };
};