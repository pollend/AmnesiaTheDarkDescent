#pragma once 

#include <engine/UUID.h>

#define HPL_RTTI_CLASS(name, id) \
    public: \
        static constexpr const char* RTTI_Name = "#name"; \
        static constexpr const UUID::UUID RTTI_ClassID = hpl::UUID::From(#id); \
        virtual const char* RTTI_GetName() { return RTTI_Name; } \
        virtual const UUID::UUID RTTI_GetClassID() { return RTTI_ClassID; } \
    private: \

#define HPL_RTTI_IMPL_CLASS(base, name, id) \
    public: \
        static constexpr const char* RTTI_Name = "#name"; \
        static constexpr const UUID::UUID RTTI_ClassID = hpl::UUID::From(#id); \
        virtual const char* RTTI_GetName() override { return RTTI_Name; } \
        virtual const UUID::UUID RTTI_GetClassID() override { return RTTI_ClassID; } \
    private: \


namespace hpl {
    template<typename T>
    class TypeInfo {
    public:
        static UUID::UUID GetClassID() {
            return T::RTTI_ClassID;
        }

        template<typename U>
        static bool isType(U& impl) {
            return impl.RTTI_GetClassID() == T::RTTI_ClassID;
        }
    };
}