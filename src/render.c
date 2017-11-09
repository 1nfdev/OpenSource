#include "render.h"
#include "texture.h"
#include "bsp.h"
#include "cache.h"
#include "common.h"
#include "profiler.h"
#include "atto/app.h"

#define RENDER_ERRORCHECK
//#define RENDER_GL_TRACE

#define RENDER_GL_PROFILE_FUNC profileEvent

#ifndef RENDER_ASSERT
#define RENDER_ASSERT(cond) \
	if (!(cond)) { \
		aAppDebugPrintf("ERROR @ %s:%d: (%s) failed", __FILE__, __LINE__, #cond); \
		aAppTerminate(-1); \
	}
#endif /* ifndef RENDER_ASSERT */

#ifdef RENDER_GL_PROFILE_FUNC
#define RENDER_GL_PROFILE_PREAMBLE const ATimeUs profile_time_start__ = aAppTime();
#define RENDER_GL_PROFILE_START const ATimeUs agl_profile_start_ = aAppTime();
#define RENDER_GL_PROFILE_END RENDER_GL_PROFILE_FUNC(__FUNCTION__, aAppTime() - agl_profile_start_);
#define RENDER_GL_PROFILE_END_NAME(name) RENDER_GL_PROFILE_FUNC(name, aAppTime() - agl_profile_start_);
#else
#define RENDER_GL_PROFILE_PREAMBLE
#define RENDER_GL_PROFILE_FUNC(...)
#endif

#if 0 //ndef RENDER_GL_DEBUG
#define GL_CALL(f) (f)
#else
#if 0
#include <stdlib.h> /* abort() */
static void a__GlPrintError(const char *message, int error) {
	const char *errstr = "UNKNOWN";
	switch (error) {
		case GL_INVALID_ENUM: errstr = "GL_INVALID_ENUM"; break;
		case GL_INVALID_VALUE: errstr = "GL_INVALID_VALUE"; break;
		case GL_INVALID_OPERATION: errstr = "GL_INVALID_OPERATION"; break;
#ifdef GL_STACK_OVERFLOW
		case GL_STACK_OVERFLOW: errstr = "GL_STACK_OVERFLOW"; break;
#endif
#ifdef GL_STACK_UNDERFLOW
		case GL_STACK_UNDERFLOW: errstr = "GL_STACK_UNDERFLOW"; break;
#endif
		case GL_OUT_OF_MEMORY: errstr = "GL_OUT_OF_MEMORY"; break;
#ifdef GL_TABLE_TOO_LARGE
		case GL_TABLE_TOO_LARGE: errstr = "GL_TABLE_TOO_LARGE"; break;
#endif
	};
	PRINTF("%s %s (%#x)", message, errstr, error);
}
#define RENDER_GL_GETERROR(f) \
		const int glerror = glGetError(); \
		if (glerror != GL_NO_ERROR) { \
			a__GlPrintError(__FILE__ ":" RENDER__GL_STR(__LINE__) ": " #f " returned ", glerror); \
			abort(); \
		}
#else
#define RENDER_GL_GETERROR(f)
#endif
#define RENDER__GL_STR__(s) #s
#define RENDER__GL_STR(s) RENDER__GL_STR__(s)
#ifdef RENDER_GL_TRACE
#define RENDER_GL_TRACE_PRINT PRINTF
#else
#define RENDER_GL_TRACE_PRINT(...)
#endif
#define GL_CALL(f) do{\
		RENDER_GL_TRACE_PRINT("%s", #f); \
		RENDER_GL_PROFILE_PREAMBLE \
		f; \
		RENDER_GL_PROFILE_FUNC(#f, aAppTime() - profile_time_start__); \
		RENDER_GL_GETERROR(f) \
	} while(0)
#endif /* RENDER_GL_DEBUG */

#ifdef _WIN32
#define WGL__FUNCLIST \
	WGL__FUNCLIST_DO(PFNGLGENBUFFERSPROC, GenBuffers) \
	WGL__FUNCLIST_DO(PFNGLBINDBUFFERPROC, BindBuffer) \
	WGL__FUNCLIST_DO(PFNGLBUFFERDATAPROC, BufferData) \
	WGL__FUNCLIST_DO(PFNGLGETATTRIBLOCATIONPROC, GetAttribLocation) \
	WGL__FUNCLIST_DO(PFNGLACTIVETEXTUREPROC, ActiveTexture) \
	WGL__FUNCLIST_DO(PFNGLCREATESHADERPROC, CreateShader) \
	WGL__FUNCLIST_DO(PFNGLSHADERSOURCEPROC, ShaderSource) \
	WGL__FUNCLIST_DO(PFNGLCOMPILESHADERPROC, CompileShader) \
	WGL__FUNCLIST_DO(PFNGLATTACHSHADERPROC, AttachShader) \
	WGL__FUNCLIST_DO(PFNGLDELETESHADERPROC, DeleteShader) \
	WGL__FUNCLIST_DO(PFNGLGETSHADERIVPROC, GetShaderiv) \
	WGL__FUNCLIST_DO(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog) \
	WGL__FUNCLIST_DO(PFNGLCREATEPROGRAMPROC, CreateProgram) \
	WGL__FUNCLIST_DO(PFNGLLINKPROGRAMPROC, LinkProgram) \
	WGL__FUNCLIST_DO(PFNGLGETPROGRAMINFOLOGPROC, GetProgramInfoLog) \
	WGL__FUNCLIST_DO(PFNGLDELETEPROGRAMPROC, DeleteProgram) \
	WGL__FUNCLIST_DO(PFNGLGETPROGRAMIVPROC, GetProgramiv) \
	WGL__FUNCLIST_DO(PFNGLUSEPROGRAMPROC, UseProgram) \
	WGL__FUNCLIST_DO(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation) \
	WGL__FUNCLIST_DO(PFNGLUNIFORM1FPROC, Uniform1f) \
	WGL__FUNCLIST_DO(PFNGLUNIFORM2FPROC, Uniform2f) \
	WGL__FUNCLIST_DO(PFNGLUNIFORM1IPROC, Uniform1i) \
	WGL__FUNCLIST_DO(PFNGLUNIFORMMATRIX4FVPROC, UniformMatrix4fv) \
	WGL__FUNCLIST_DO(PFNGLENABLEVERTEXATTRIBARRAYPROC, EnableVertexAttribArray) \
	WGL__FUNCLIST_DO(PFNGLVERTEXATTRIBPOINTERPROC, VertexAttribPointer) \
	WGL__FUNCLIST_DO(PFNGLGENERATEMIPMAPPROC, GenerateMipmap) \

#define WGL__FUNCLIST_DO(T,N) T gl##N = 0;
WGL__FUNCLIST
#undef WGL__FUNCLIST_DO
#endif /* ifdef _WIN32 */

typedef GLint RProgram;

static GLint render_ShaderCreate(GLenum type, const char *sources[]) {
	int n;
	GLuint shader = glCreateShader(type);

	for (n = 0; sources[n]; ++n);

	GL_CALL(glShaderSource(shader, n, (const GLchar **)sources, 0));
	GL_CALL(glCompileShader(shader));

#ifdef RENDER_ERRORCHECK
	{
		GLint status;
		GL_CALL(glGetShaderiv(shader, GL_COMPILE_STATUS, &status));
		if (status != GL_TRUE) {
			char buffer[1024];
			GL_CALL(glGetShaderInfoLog(shader, sizeof(buffer), 0, buffer));
			PRINTF("Shader compilation error: %s", buffer);
			GL_CALL(glDeleteShader(shader));
			shader = 0;
		}
	}
#endif

	return shader;
}

static RProgram render_ProgramCreate(const char *common, const char *vertex, const char *fragment) {
	GLuint program;
	GLuint vertex_shader, fragment_shader;
	const char *sources[] = {
		common, fragment, 0
	};
	fragment_shader = render_ShaderCreate(GL_FRAGMENT_SHADER, sources);
	if (fragment_shader == 0)
		return -1;

	sources[1] = vertex;
	vertex_shader = render_ShaderCreate(GL_VERTEX_SHADER, sources);
	if (vertex_shader == 0) {
		GL_CALL(glDeleteShader(fragment_shader));
		return -2;
	}

	program = glCreateProgram();
	GL_CALL(glAttachShader(program, fragment_shader));
	GL_CALL(glAttachShader(program, vertex_shader));
	GL_CALL(glLinkProgram(program));

	GL_CALL(glDeleteShader(fragment_shader));
	GL_CALL(glDeleteShader(vertex_shader));

#ifdef RENDER_ERRORCHECK
	{
		GLint status;
		GL_CALL(glGetProgramiv(program, GL_LINK_STATUS, &status));
		if (status != GL_TRUE) {
			char buffer[1024];
			GL_CALL(glGetProgramInfoLog(program, sizeof(buffer), 0, buffer));
			PRINTF("Program linking error: %s", buffer);
			GL_CALL(glDeleteProgram(program));
			return -3;
		}
	}
#endif

	return program;
}

void renderTextureCreate(RTexture *texture, RTextureCreateParams params) {
	GLenum internal, format, type;
	GL_CALL(glGenTextures(1, &texture->gl_name));

	switch (params.format) {
		case RTexFormat_RGB565:
			internal = format = GL_RGB; type = GL_UNSIGNED_SHORT_5_6_5; break;
	}
	GL_CALL(glBindTexture(GL_TEXTURE_2D, texture->gl_name));

	GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, internal, params.width, params.height, 0,
			format, type, params.pixels));

	GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));

	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
	GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));

	texture->width = params.width;
	texture->height = params.height;
	texture->format = params.format;
}

void renderBufferCreate(RBuffer *buffer, RBufferType type, int size, const void *data) {
	GL_CALL(glGenBuffers(1, &buffer->gl_name));
	GL_CALL(glBindBuffer(type, buffer->gl_name));
	GL_CALL(glBufferData(type, size, data, GL_STATIC_DRAW));
}

static const char lmgen_common_src[] =
	"varying vec2 v_lightmap_uv, v_tex_uv;\n"
	"varying vec3 v_normal;\n";

static const char lmgen_vertex_src[] =
	"attribute vec3 a_vertex, a_normal;\n"
	"attribute vec2 a_lightmap_uv, a_tex_uv;\n"
	"uniform mat4 u_mvp;\n"
	"void main() {\n"
		"v_lightmap_uv = a_lightmap_uv;\n"
		"v_tex_uv = a_tex_uv;\n"
		"v_normal = a_normal;\n"
		"gl_Position = u_mvp * vec4(a_vertex, 1.);\n"
	"}\n";

static const char lmgen_fragment_src[] =
	"uniform sampler2D u_lightmap, u_tex0, u_tex1;\n"
	"uniform vec2 u_lightmap_size, u_tex0_size, u_tex1_size;\n"
	"uniform float u_lmn;\n"
	"void main() {\n"
		"vec3 tc = vec3(fract(v_tex_uv/u_tex0_size), 0.);\n"
		"vec4 albedo = texture2D(u_tex0, v_tex_uv/u_tex0_size);\n"
		"albedo = mix(albedo, texture2D(u_tex1, v_tex_uv/u_tex1_size), .0);\n"
		"vec3 lm = texture2D(u_lightmap, v_lightmap_uv).xyz;\n"
		"vec3 color = albedo.xyz * (vec3(.1) + lm);\n"
		"gl_FragColor = vec4(mix(color, tc, u_lmn), 1.);\n"
	"}\n";

typedef struct {
	const char *name;
	int components;
	GLenum type;
	int stride;
	const void *ptr;
	int location;
} RAttrib;

typedef struct {
	const char *name;
	int location;
} RUniform;

static RAttrib lmgen_attribs[] = {
	{"a_vertex", 3, GL_FLOAT, sizeof(struct BSPModelVertex), (void*)offsetof(struct BSPModelVertex, vertex), -1},
	{"a_normal", 3, GL_FLOAT, sizeof(struct BSPModelVertex), (void*)offsetof(struct BSPModelVertex, normal), -1},
	{"a_lightmap_uv", 2, GL_FLOAT, sizeof(struct BSPModelVertex), (void*)offsetof(struct BSPModelVertex, lightmap_uv), -1},
	{"a_tex_uv", 2, GL_FLOAT, sizeof(struct BSPModelVertex), (void*)offsetof(struct BSPModelVertex, tex_uv), -1},
	{0, 0, 0, 0, 0, -1}
};

static RUniform lmgen_uniforms[] = {
	{"u_mvp", -1},
	{"u_lmn", -1},
	{"u_lightmap", -1},
	{"u_tex0", -1},
	{"u_tex0_size", -1},
	{"u_tex1", -1},
	{"u_tex1_size", -1},
	{0, -1}
};

static struct {
	RProgram lmgen_program;
	const RTexture *current_tex0;
} r;

static void renderLocateAttribs(RProgram prog, RAttrib *attribs) {
	for(int i = 0; attribs[i].name; ++i) {
		attribs[i].location = glGetAttribLocation(prog, attribs[i].name);
		if (attribs[i].location < 0)
			PRINTF("Cannot locate attribute %s", attribs[i].name);
	}
}

static void renderLocateUniforms(RProgram prog, RUniform *uniforms) {
	for(int i = 0; uniforms[i].name; ++i) {
		uniforms[i].location = glGetUniformLocation(prog, uniforms[i].name);
		if (uniforms[i].location < 0)
			PRINTF("Cannot locate uniform %s", uniforms[i].name);
	}
}

static void renderApplyAttribs(const RAttrib *attribs, const RBuffer *buffer, unsigned int vbo_offset) {
	for(int i = 0; attribs[i].name; ++i) {
		const RAttrib *a = attribs + i;
		if (a->location < 0) continue;
		GL_CALL(glEnableVertexAttribArray(a->location));
		GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, buffer->gl_name));
		GL_CALL(glVertexAttribPointer(a->location, a->components, a->type, GL_FALSE, a->stride, (const char*)a->ptr + vbo_offset * sizeof(struct BSPModelVertex)));
	}
}

int renderInit() {
#ifdef _WIN32
#define WGL__FUNCLIST_DO(T, N) \
	gl##N = (T)wglGetProcAddress("gl" #N); \
	ASSERT(gl##N);

	WGL__FUNCLIST
#undef WGL__FUNCLIST_DO
#endif

	r.lmgen_program = render_ProgramCreate(lmgen_common_src, lmgen_vertex_src, lmgen_fragment_src);
	if (r.lmgen_program <= 0) {
		PRINT("Cannot create program for lightmapped generic material");
		return 0;
	}

	renderLocateAttribs(r.lmgen_program, lmgen_attribs);
	renderLocateUniforms(r.lmgen_program, lmgen_uniforms);

	struct Texture default_texture;
	RTextureCreateParams params;
	params.format = RTexFormat_RGB565;
	params.width = 1;
	params.height = 1;
	params.pixels = (uint16_t[]){0xffff};
	renderTextureCreate(&default_texture.texture, params);
	cachePutTexture("opensource/placeholder", &default_texture);

	struct Material default_material;
	memset(&default_material, 0, sizeof default_material);
	default_material.base_texture[0] = cacheGetTexture("opensource/placeholder");
	cachePutMaterial("opensource/placeholder", &default_material);

	GL_CALL(glEnable(GL_DEPTH_TEST));
	GL_CALL(glEnable(GL_CULL_FACE));
	r.current_tex0 = 0;
	return 1;
}

static void renderDraw(const struct BSPDraw *draw) {
	const struct Material *m = draw->material;
	if (m->base_texture[0]) {
		const RTexture *t = &m->base_texture[0]->texture;
		if (t != r.current_tex0) {
			GL_CALL(glBindTexture(GL_TEXTURE_2D, t->gl_name));
			GL_CALL(glUniform2f(lmgen_uniforms[4].location, (float)t->width, (float)t->height));
			r.current_tex0 = t;
		}
	}

	GL_CALL(glDrawElements(GL_TRIANGLES, draw->count, GL_UNSIGNED_SHORT, (void*)(sizeof(uint16_t) * draw->start)));
}

static void renderDrawSet(const struct BSPModel *model, const struct BSPDrawSet *drawset) {
	unsigned int vbo_offset = 0;
	for (int i = 0; i < drawset->draws_count; ++i) {
		const struct BSPDraw *draw = drawset->draws + i;
		if (i == 0 || draw->vbo_offset != vbo_offset) {
			vbo_offset = draw->vbo_offset;
			renderApplyAttribs(lmgen_attribs, &model->vbo, draw->vbo_offset);
		}
		renderDraw(draw);
	}
}

static float aMaxf(float a, float b) { return a > b ? a : b; }
//static float aMinf(float a, float b) { return a < b ? a : b; }

void renderModelDraw(const struct AMat4f *mvp, struct AVec3f camera_position, float lmn, const struct BSPModel *model) {
	if (!model->detailed.draws_count) return;

	GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->ibo.gl_name));
	GL_CALL(glUseProgram(r.lmgen_program));
	GL_CALL(glActiveTexture(GL_TEXTURE0));
	GL_CALL(glBindTexture(GL_TEXTURE_2D, model->lightmap.gl_name));

	GL_CALL(glUniformMatrix4fv(lmgen_uniforms[0].location, 1, GL_FALSE, &mvp->X.x));
	GL_CALL(glUniform1f(lmgen_uniforms[1].location, lmn));
	GL_CALL(glUniform1i(lmgen_uniforms[2].location, 0));
	GL_CALL(glUniform1i(lmgen_uniforms[3].location, 1));
	GL_CALL(glUniform1i(lmgen_uniforms[5].location, 2));

	GL_CALL(glActiveTexture(GL_TEXTURE0 + 1));

	const float distance =
		aMaxf(aMaxf(
			aMaxf(camera_position.x - model->aabb.max.x, model->aabb.min.x - camera_position.x),
			aMaxf(camera_position.y - model->aabb.max.y, model->aabb.min.y - camera_position.y)),
			aMaxf(camera_position.z - model->aabb.max.z, model->aabb.min.z - camera_position.z));

	/*
	PRINTF("%f %f %f -> %f",
			camera_position.x, camera_position.y, camera_position.z, distance);
	*/

	if (distance < 5000.f)
		renderDrawSet(model, &model->detailed);
	else
		renderDrawSet(model, &model->coarse);
}

void renderClear() {
	glClearColor(.5f,.4f,.2f,0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
