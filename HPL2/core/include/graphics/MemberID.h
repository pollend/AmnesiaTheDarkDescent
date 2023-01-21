#pragma once

#include "math/MathTypes.h"
#include "system/SystemTypes.h"
#include <functional>
namespace hpl
{
    class MemberID
    {
    public:
        MemberID(const tString& name, int id)
            : _id(id)
            , _name(name)
        {
        }

        MemberID(const tString& name)
            : _id(std::hash<tString>()(name))
            , _name(name)
        {
        }

        MemberID()
            : _id(-1)
            , _name()
        {
        }

        const tString& name() const
        {
            return _name;
        }

        int id() const
        {
            return _id;
        }

    private:
        int _id;
        tString _name;
    };
} // namespace hpl