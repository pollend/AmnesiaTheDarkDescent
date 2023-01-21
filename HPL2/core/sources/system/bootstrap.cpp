#include <system/bootstrap.h>

#include <bx/file.h>

namespace hpl
{
    namespace bootstrap
    {
        static bx::FileReaderI* s_fileReader = nullptr;
        static bx::FileWriterI* s_fileWriter = nullptr;

        void Init() {
            s_fileReader = new bx::FileReader();
            s_fileWriter = new bx::FileWriter();
        }

        bx::FileReaderI* GetFileReader() {
            return s_fileReader;
        }

        bx::FileWriterI* GetFileWriter() {
            return s_fileWriter;
        }


    } // namespace bootstrap
} // namespace hpl