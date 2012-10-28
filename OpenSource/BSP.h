#pragma once


namespace kapusha {
class StreamSeekable;
}

using kapusha::StreamSeekable;

class BSP
{
public:
  BSP(void);
  ~BSP(void);

  bool load(StreamSeekable *stream);

  void draw() const;

private:
  class Impl;
  std::auto_ptr<Impl> pimpl_;
};