#pragma once

#include <vector>
#include <map>
#include <memory>


namespace kapusha {
  class StreamSeekable;
  class Camera;
  class Object;
}

using kapusha::StreamSeekable;
using kapusha::Object;

class Materializer;

class BSP
{
public:
  BSP(void);
  ~BSP(void);

  bool load(StreamSeekable *stream, Materializer* materializer);

  void setParent(const BSP* parent, math::vec3f relative);
  void updateShift(math::vec3f shift);
  const math::vec3f& shift() const { return shift_; }
  const math::vec3f& translation() const { return translation_; }
  
  void setShowBounds(bool show) { show_bbox_ = show; }
  void draw(const kapusha::Camera&) const;

public:
  struct MapLink {
    std::map<std::string, math::vec3f> landmarks;
    std::map<std::string, std::string> maps;
  };
  const MapLink& getMapLinks() const { return links_; }

private:
  const BSP *parent_;
  math::vec3f relative_;
  math::vec3f translation_;
  math::vec3f shift_;
  MapLink links_;
  std::vector<Object*> objects_;
  bool show_bbox_;
  Object *edges_;
  Object *bbox_; //! \todo
};
