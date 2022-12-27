#pragma once

#include <bx/file.h>

namespace hpl
{
    namespace bootstrap
    {

        static bx::FileReaderI* s_fileReader = NULL;
        static bx::FileWriterI* s_fileWriter = NULL;

        class FileReader : public bx::FileReader
        {
        };
        class FileWriter : public bx::FileWriter
        {
        };

        void Init()
        {
            s_fileReader = new FileReader();
            s_fileWriter = new FileWriter();
        }

        bx::FileReaderI* GetFileReader()
        {
            return s_fileReader;
        }

        bx::FileWriterI* GetFileWriter()
        {
            return s_fileWriter;
        }

    } // namespace bootstrap
} // namespace hpl