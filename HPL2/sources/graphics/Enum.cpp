#include <graphics/Enum.h>

namespace hpl {

    ShaderSemantic hplToForgeShaderSemantic(eVertexBufferElement element) {
        switch (element) {
        case eVertexBufferElement_Normal:
            return ShaderSemantic::SEMANTIC_NORMAL;
        case eVertexBufferElement_Position:
            return ShaderSemantic::SEMANTIC_POSITION;
        case eVertexBufferElement_Color0:
            return ShaderSemantic::SEMANTIC_COLOR;
        case eVertexBufferElement_Color1:
            return ShaderSemantic::SEMANTIC_TEXCOORD5;
        case eVertexBufferElement_Texture1Tangent:
            return ShaderSemantic::SEMANTIC_TANGENT;
        case eVertexBufferElement_Texture0:
            return ShaderSemantic::SEMANTIC_TEXCOORD0;
        case eVertexBufferElement_Texture1:
            return ShaderSemantic::SEMANTIC_TEXCOORD1;
        case eVertexBufferElement_Texture2:
            return ShaderSemantic::SEMANTIC_TEXCOORD2;
        case eVertexBufferElement_Texture3:
            return ShaderSemantic::SEMANTIC_TEXCOORD3;
        case eVertexBufferElement_Texture4:
            return ShaderSemantic::SEMANTIC_TEXCOORD4;
        case eVertexBufferElement_User0:
            return ShaderSemantic::SEMANTIC_TEXCOORD6;
        case eVertexBufferElement_User1:
            return ShaderSemantic::SEMANTIC_TEXCOORD7;
        case eVertexBufferElement_User2:
            return ShaderSemantic::SEMANTIC_TEXCOORD8;
        case eVertexBufferElement_User3:
            return ShaderSemantic::SEMANTIC_TEXCOORD9;
        }
        return ShaderSemantic::SEMANTIC_UNDEFINED;
    }
}
