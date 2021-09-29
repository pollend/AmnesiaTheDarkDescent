#pragma once
#include "Common_3/Renderer/IRenderer.h"

namespace hpl {
template <typename TStruct> class HPLParameterStruct {
public:
  HPLParameterStruct() {}
  ~HPLParameterStruct() {}

  void setBuffer(Buffer *buffer) { _buffer = buffer; }
  TStruct &get() { return _data; }

private:
  TStruct _data;
  Buffer *_buffer;
};
}