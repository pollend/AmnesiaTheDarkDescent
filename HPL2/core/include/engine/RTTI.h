
#include <engine/UUID.h>

#define HPL_RTTI_CLASS(name, id) \
    public: \
        static constexpr const char* Name = "#name"; \
        static constexpr const UUID ClassID = hpl::UUid::FromCString( "#id" ); \ 
    private: