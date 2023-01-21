#pragma once

#include <bx/file.h>

namespace hpl
{
    namespace bootstrap
    {        
    class FileReader : public bx::FileReader
        {
        };
        class FileWriter : public bx::FileWriter
        {
        };

        void Init();
        bx::FileReaderI* GetFileReader();
        bx::FileWriterI* GetFileWriter();

    } // namespace bootstrap
} // namespace hpl