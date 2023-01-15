
#include <engine/UUID.h>

#define HPL_RTTI_CLASS(name, id) \
    public: \
        static const char* GetRTTIName() { return #name; } \
        static const UUid GetId() { return hpl::UUid::FromCString( #id ); } \ 
    private: