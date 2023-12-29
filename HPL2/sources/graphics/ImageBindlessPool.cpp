#include "graphics/ImageBindlessPool.h"
#include "graphics/Image.h"
#include "graphics/IndexPool.h"
#include "graphics/BindlessDescriptorPool.h"

#include <cstdint>

#include <folly/hash/Hash.h>

namespace hpl {
    ImageBindlessPool::ImageBindlessPool() {

    }
    ImageBindlessPool::ImageBindlessPool(BindlessDescriptorPool* pool, uint32_t minimumReserve):
        m_texturePool(pool),
        m_allocSize(minimumReserve) {
        m_pool.resize(m_allocSize * 1.5f);
    }


    void ImageBindlessPool::reset(const ForgeRenderer::Frame& frame) {
        m_currentFrame = frame.m_currentFrame;
        while(m_headIndex != UINT16_MAX &&
            (m_currentFrame - m_pool[m_headIndex].m_frameCount) >= MinimumFrameCount) {
            auto& head = m_pool[m_headIndex];
            // free data
            m_texturePool->dispose(head.m_handle);
            head.m_handle = IndexPool::InvalidHandle;
            head.m_image = nullptr;

            // move the head back
            m_headIndex = head.m_prev;

            // clear the next and previous pointers
            head.m_next = UINT16_MAX;
            head.m_prev = UINT16_MAX;

            if(m_headIndex != UINT16_MAX) {
                m_pool[m_headIndex].m_next = UINT16_MAX;
            } else {
                m_tailIndex = UINT16_MAX;
            }
        }
    }
    ImageBindlessPool::~ImageBindlessPool() {
        while(m_headIndex != UINT16_MAX) {
            auto& head = m_pool[m_headIndex];
            m_texturePool->dispose(head.m_handle);

            // move the head back
            m_headIndex = head.m_prev;
        }

    }

    uint32_t ImageBindlessPool::request(Image* image) {
        ASSERT(m_texturePool);
        uint32_t targetIndex = folly::hash::fnv32_buf(image, sizeof(Image*)) % m_pool.size();
        auto refreshAppendSlot = [&](uint32_t index) {
            auto& current = m_pool[index];

            if (m_currentFrame == current.m_frameCount ||
                m_tailIndex == index ||
                (m_headIndex == index && m_tailIndex == index)) {
                current.m_frameCount = m_currentFrame;
                return;
            } else if (m_headIndex == index) {
                m_headIndex = current.m_prev;
            }

            if(current.m_prev != UINT16_MAX) {
                m_pool[current.m_prev].m_next = current.m_next;
            }
            if(current.m_next != UINT16_MAX) {
                m_pool[current.m_next].m_prev = current.m_prev;
            }
            current.m_prev = UINT16_MAX;
            current.m_next = UINT16_MAX;
            current.m_frameCount = m_currentFrame;
            if(m_tailIndex != UINT16_MAX) {
                current.m_next = m_tailIndex;
                m_pool[m_tailIndex].m_prev = index;
                m_tailIndex = index;
            } else {
                m_tailIndex = index;
                m_headIndex = index;
            }

        };

        uint32_t index = targetIndex;
        do {
            auto& current = m_pool[index];
            if(current.m_image == image) {
                refreshAppendSlot(index);
                return current.m_handle;
            } else if(current.m_image == nullptr) {
                current.m_image = image;
                current.m_handle = m_texturePool->request(image->GetTexture());
                refreshAppendSlot(index);
                return current.m_handle;
            }
            index = (index + 1) % m_pool.size();
        } while(index != targetIndex);
        return IndexPool::InvalidHandle;
    }
} // namespace hpl
