#pragma once

#include <memory>

namespace hpl {

    class HandleWrapper {
        public:
            using Ptr = std::unique_ptr<void, void(*)(void*)>;

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

            void* Get() {
                return m_ptr.get();
            }
        private:
            std::unique_ptr<void, void(*)(void*)> m_ptr; // debating whether to use shared_ptr or unique_ptr
    };
};