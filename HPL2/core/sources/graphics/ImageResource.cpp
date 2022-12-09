
#include <graphics/ImageResource.h>

namespace hpl
{

    ImageResource::ImageResource(const tString& asName, const tWString& asFullPath)
        : iResourceBase(asName, asFullPath, 0)
    {
    }

    bool ImageResource::Reload()
    {
        return false;
    }
    void ImageResource::Unload()
    {
    }
    void ImageResource::Destroy()
    {
    }

    ImageResource::~ImageResource()
    {
    }

} // namespace hpl