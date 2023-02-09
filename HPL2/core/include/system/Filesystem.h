#pragma once

#include <bx/file.h>
#include <engine/RTTI.h>

namespace hpl {
    class FileReader : public bx::FileReader{
        HPL_RTTI_CLASS(FileReader, "{ea6b3aa2-5ab4-478a-829a-260428ba67d2}")
    };

    class FileWriter : public bx::FileWriter {
        HPL_RTTI_CLASS(FileWriter, "{d55f1a3b-3754-44c3-a00f-956c4c8a45d8}")
    };

}