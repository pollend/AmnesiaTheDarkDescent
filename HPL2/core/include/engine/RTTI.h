#pragma once 

#include <engine/UUID.h>

#define HPL_RTTI_CLASS(name, id) \
    public: \
        static constexpr const char* Name = "#name"; \
        static constexpr const UUID::UUID ClassID = hpl::UUID::From(#id); \
        virtual const char* GetName() { return Name; } \
        virtual const UUID::UUID GetClassID() { return ClassID; } \
    private:

namespace hpl {
    template<typename T>
    class TypeInfo {
    public:
        static bool GetClassID() {
            return T::ClassID;
        }

        template<typename U>
        static bool isType(U& impl) {
            return impl.GetClassID() == T::ClassID;
        }
    };
}