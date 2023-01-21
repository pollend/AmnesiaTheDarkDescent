#include "bgfx/bgfx.h"
#include <graphics/ShaderVariantCollection.h> 
#include <bx/string.h>
#include <memory>

namespace hpl::ShaderHelper {

    std::function<ShaderVariant(uint32_t flags)> LoadProgramHandlerDefault(std::string_view vs, std::string_view fs, bool isVariantVertex, bool isVariantFragment)
    {
        return [vs, fs, isVariantVertex, isVariantFragment](uint32_t flags) -> hpl::ShaderVariant {
            char vs_name[1024] = {0};
            char fs_name[1024] = {0};
            
            if(isVariantVertex) {
                bx::snprintf(vs_name, std::size(vs_name), "%s_%d", vs.data(), flags);
            } else {
                bx::strCopy(vs_name, std::size(vs_name), vs.data());
            }
            if(isVariantFragment) {
                bx::snprintf(fs_name, std::size(fs_name), "%s_%d", fs.data(), flags);
            } else {
                bx::strCopy(fs_name, std::size(fs_name), fs.data());
            }
            return {
                hpl::loadProgram(vs_name, fs_name),
                [](ShaderVariant& variant) {
                    if (bgfx::isValid(variant.m_programHandle))
                    {
                        bgfx::destroy(variant.m_programHandle);
                    }
                }
            
            };
        };
    }

}