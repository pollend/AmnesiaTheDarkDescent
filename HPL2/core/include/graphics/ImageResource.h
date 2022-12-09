#pragma once

#include <graphics/Image.h>
#include <resources/ResourceBase.h>

namespace hpl {
    class ImageResource : public iResourceBase {
        public:
            ImageResource(const tString& asName, const tWString& asFullPath);
            ~ImageResource();

            virtual bool Reload() override;
            virtual void Unload() override;
            virtual void Destroy() override;

            Image& GetImage() { return m_image; }
        private:
            Image m_image;
    };
}