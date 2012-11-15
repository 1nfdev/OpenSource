#include <Kapusha/sys/Log.h>
#include <Kapusha/io/Stream.h>
#include <Kapusha/gl/Program.h>
#include <Kapusha/gl/Material.h>
#include <Kapusha/gl/Texture.h>
#include "ResRes.h"
#include "VTF.h"
#include "Materializer.h"

static const char* shader_vertex =
  "uniform mat4 um4_view, um4_proj;\n"
  "uniform vec4 uv4_trans;\n"
  "uniform vec2 uv2_texscale;\n"
  "attribute vec4 av4_vtx, av4_tex;\n"
  "varying vec2 vv2_tex;\n"
  "void main(){\n"
    "gl_Position = um4_proj * um4_view * (av4_vtx + uv4_trans);\n"
    "vv2_tex = av4_tex * uv2_texscale;\n"
  "}"
;

static const char* shader_fragment =
  "uniform sampler2D us2_texture;\n"
  "varying vec2 vv2_tex;\n"
  "void main(){\n"
    "gl_FragColor = texture2D(us2_texture, vv2_tex);\n"
  "}"
;

static const char* shader_vertex_colored =
  "uniform mat4 um4_view, um4_proj;\n"
  "uniform vec4 uv4_trans;\n"
  "attribute vec4 av4_vertex;\n" //, av4_color;\n"
  "attribute vec2 av2_lightmap;\n"
  //"uniform vec2 uv2_texscale;\n"
  //"varying vec4 vv4_color;\n"
  "varying vec2 vv2_lightmap;\n"
  "void main(){\n"
    "gl_Position = um4_proj * um4_view * (av4_vertex + uv4_trans);\n"
    //"vv4_color = av4_color;\n"
    "vv2_lightmap = av2_lightmap;\n"
  "}"
;

static const char* shader_fragment_colored =
  //"varying vec4 vv4_color;\n"
  "uniform sampler2D us2_lightmap;\n"
  "varying vec2 vv2_lightmap;\n"
  "void main(){\n"
    "gl_FragColor = texture2D(us2_lightmap, vv2_lightmap);\n"
    //"gl_FragColor = vv4_color;\n"
  "}"
;

Materializer::Materializer(const ResRes& resources)
  : resources_(resources)
{
  UBER_SHADER1111_ = new kapusha::Program(shader_vertex,
                                          shader_fragment);
}

Materializer::~Materializer(void)
{
}

kapusha::Material* Materializer::loadMaterial(const char *name_raw)
{
  std::string name(name_raw);

  //! \fixme broken atm
  KP_ASSERT(name != "__BSP_edge");

  auto fm = cached_materials_.find(name);
  if (fm != cached_materials_.end())
    return fm->second;

  if (name == "__colored_vertex")
  {
    kapusha::Program *prog = new kapusha::Program(shader_vertex_colored,
                                                  shader_fragment_colored);
    kapusha::Material* mat = new kapusha::Material(prog);
    cached_materials_[name] = mat;
    return mat;
  }

  kapusha::StreamSeekable* restream = resources_.open(name.c_str(), ResRes::ResourceTexture);
  kapusha::Material* mat = new kapusha::Material(UBER_SHADER1111_);
  if (restream)
  {
    VTF tex;
    kapusha::Texture *texture = tex.load(*restream);
    if (texture)
    {
      mat->setUniform("uv2_texscale",
                      math::vec2f(1.f / tex.size().x, 1.f / tex.size().y));
      mat->setTexture("us2_texture", texture);
    }
    delete restream;
  }
  
  cached_materials_[name] = mat;

  return mat;
}

void Materializer::print() const
{
  L("Cached materials:");
  int i = 0;
  for (auto it = cached_materials_.begin();
    it != cached_materials_.end(); ++it, ++i)
    L("\t%d: %s", i, it->first.c_str());
  L("TOTAL: %d", cached_materials_.size());
}