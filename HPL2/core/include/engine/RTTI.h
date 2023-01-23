
#include <engine/UUID.h>

#define HPL_RTTI_CLASS(name, id) \
    public: \
        static constexpr const char* Name = "#name"; \
        static constexpr const UUID::UUID ClassID = hpl::UUID::From(#id); \
    private:

