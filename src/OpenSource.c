#include "bsp.h"
#include "cache.h"
#include "collection.h"
#include "mempools.h"
#include "common.h"
#include "texture.h"

#include "atto/app.h"

void profileEvent(const char *msg, ATimeUs delta);

//#define ATTO_GL_PROFILE_FUNC profileEvent
//#define ATTO_GL_TRACE
//#define ATTO_GL_DEBUG
//#define ATTO_GL_H_IMPLEMENT
//#include "atto/gl.h"
#include "atto/math.h"

static char persistent_data[32*1024*1024];
static char temp_data[32*1024*1024];

static struct Stack stack_temp = {
	.storage = temp_data,
	.size = sizeof(temp_data),
	.cursor = 0
};

static struct Stack stack_persistent = {
	.storage = persistent_data,
	.size = sizeof(persistent_data),
	.cursor = 0
};

struct SimpleCamera {
	struct AVec3f pos, dir, up;
	struct AMat3f axes;
	struct AMat4f projection;
	struct AMat4f view_projection;
};

static void simplecamRecalc(struct SimpleCamera *cam) {
	cam->dir = aVec3fNormalize(cam->dir);
	const struct AVec3f
			z = aVec3fNeg(cam->dir),
			x = aVec3fNormalize(aVec3fCross(cam->up, z)),
			y = aVec3fCross(z, x);
	cam->axes = aMat3fv(x, y, z);
	const struct AMat3f axes_inv = aMat3fTranspose(cam->axes);
	cam->view_projection = aMat4fMul(cam->projection,
		aMat4f3(axes_inv, aVec3fMulMat(axes_inv, aVec3fNeg(cam->pos))));
}

static void simplecamProjection(struct SimpleCamera *cam, float near, float far, float horizfov, float aspect) {
	const float w = 2.f * near * tanf(horizfov / 2.f), h = w / aspect;
	//aAppDebugPrintf("%f %f %f %f -> %f %f", near, far, horizfov, aspect, w, h);
	cam->projection = aMat4fPerspective(near, far, w, h);
}

static void simplecamLookAt(struct SimpleCamera *cam, struct AVec3f pos, struct AVec3f at, struct AVec3f up) {
	cam->pos = pos;
	cam->dir = aVec3fNormalize(aVec3fSub(at, pos));
	cam->up = up;
	simplecamRecalc(cam);
}

static void simplecamMove(struct SimpleCamera *cam, struct AVec3f v) {
	cam->pos = aVec3fAdd(cam->pos, aVec3fMulf(cam->axes.X, v.x));
	cam->pos = aVec3fAdd(cam->pos, aVec3fMulf(cam->axes.Y, v.y));
	cam->pos = aVec3fAdd(cam->pos, aVec3fMulf(cam->axes.Z, v.z));
}

static void simplecamRotateYaw(struct SimpleCamera *cam, float yaw) {
	const struct AMat3f rot = aMat3fRotateAxis(cam->up, yaw);
	cam->dir = aVec3fMulMat(rot, cam->dir);
}

static void simplecamRotatePitch(struct SimpleCamera *cam, float pitch) {
	/* TODO limit pitch */
	const struct AMat3f rot = aMat3fRotateAxis(cam->axes.X, pitch);
	cam->dir = aVec3fMulMat(rot, cam->dir);
}

#define COUNTOF(v) (sizeof(v) / sizeof(*(v)))

static struct {
	int cursor;
	struct {
		const char *msg;
		ATimeUs delta;
	} event[65536];
	int frame;
	ATimeUs last_print_time;
	ATimeUs profiler_time;
	ATimeUs frame_deltas, last_frame;
	int counted_frame;
} profiler;

static void profilerInit() {
	profiler.cursor = 0;
	profiler.frame = 0;
	profiler.last_print_time = 0;
	profiler.profiler_time = 0;
	profiler.frame_deltas = profiler.last_frame = 0;
	profiler.counted_frame = 0;
}

void profileEvent(const char *msg, ATimeUs delta) {
	ATTO_ASSERT(profiler.cursor < 65536);
	profiler.event[profiler.cursor].msg = msg;
	profiler.event[profiler.cursor].delta = delta;
	++profiler.cursor;
}

typedef struct {
	const char *name;
	int count;
	ATimeUs total_time;
	ATimeUs min_time;
	ATimeUs max_time;
} ProfilerLocation;

static int profilerFrame() {
	int retval = 0;
	const ATimeUs start = aAppTime();
	profiler.frame_deltas += start - profiler.last_frame;
	
	void *tmp_cursor = stackGetCursor(&stack_temp);
	const int max_array_size = stackGetFree(&stack_temp) / sizeof(ProfilerLocation);
	int array_size = 0;
	ProfilerLocation *array = tmp_cursor;
	int total_time = 0;
	for (int i = 0; i < profiler.cursor; ++i) {
		ProfilerLocation *loc = 0;
		for (int j = 0; j < array_size; ++j)
			if (array[j].name == profiler.event[i].msg) {
				loc = array + j;
				break;
			}
		if (!loc) {
			ATTO_ASSERT(array_size< max_array_size);
			loc = array + array_size++;
			loc->name = profiler.event[i].msg;
			loc->count = 0;
			loc->total_time = 0;
			loc->min_time = 0x7fffffffu;
			loc->max_time = 0;
		}

		++loc->count;
		const ATimeUs delta = profiler.event[i].delta;
		loc->total_time += delta;
		total_time += delta;
		if (delta < loc->min_time) loc->min_time = delta;
		if (delta > loc->max_time) loc->max_time = delta;
	}

	++profiler.counted_frame;
	++profiler.frame;

	if (start - profiler.last_print_time > 60000000) {
		PRINT("=================================================");
		const ATimeUs dt = profiler.frame_deltas / profiler.counted_frame;
		PRINTF("avg frame = %dus (fps = %f)", dt, 1000000. / dt);
		PRINTF("PROF: frame=%d, total_frame_time=%d total_prof_time=%d, avg_prof_time=%d events=%d unique=%d",
			profiler.frame, total_time, profiler.profiler_time, profiler.profiler_time / profiler.frame,
			profiler.cursor, array_size);

	for (int i = 0; i < array_size; ++i) {
		const ProfilerLocation *loc = array + i;
		PRINTF("T%d: total=%d count=%d min=%d max=%d, avg=%d %s",
				i, loc->total_time, loc->count, loc->min_time, loc->max_time,
				loc->total_time / loc->count, loc->name);
	}

#if 0
#define TOP_N 10
		int max_time[TOP_N] = {0};
		int max_count[TOP_N] = {0};
		for (int i = 0; i < array_size; ++i) {
			const ProfilerLocation *loc = array + i;
			for (int j = 0; j < TOP_N; ++j)
				if (array[max_time[j]].max_time < loc->max_time) {
					for (int k = j + 1; k < TOP_N; ++k) max_time[k] = max_time[k - 1];
					max_time[j] = i;
					break;
				}
			for (int j = 0; j < TOP_N; ++j)
				if (array[max_count[j]].count < loc->count) {
					for (int k = j + 1; k < TOP_N; ++k) max_count[k] = max_count[k - 1];
					max_count[j] = i;
					break;
				}
		}
		if (array_size > TOP_N) {
			for (int i = 0; i < TOP_N; ++i) {
				const ProfilerLocation *loc = array + max_time[i];
				PRINTF("T%d %d: total=%d count=%d min=%d max=%d, avg=%d %s",
						i, max_time[i], loc->total_time, loc->count, loc->min_time, loc->max_time,
						loc->total_time / loc->count, loc->name);
			}
			for (int i = 0; i < TOP_N; ++i) {
				const ProfilerLocation *loc = array + max_count[i];
				PRINTF("C%d %d: total=%d count=%d min=%d max=%d, avg=%d %s",
						i, max_count[i], loc->total_time, loc->count, loc->min_time, loc->max_time,
						loc->total_time / loc->count, loc->name);
			}
		}
#endif

		profiler.last_print_time = start;
		profiler.counted_frame = 0;
		profiler.frame_deltas = 0;
		retval = 1;
	}

	profiler.last_frame = start;
	profiler.profiler_time += aAppTime() - start;
	profiler.cursor = 0;
	profileEvent("PROFILER", aAppTime() - start);
	return retval;
}

#if 0
static const float fsquad_vertices[] = {
	-1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f, -1.f
};

static const char fsquad_vertex_src[] =
	"attribute vec2 av2_pos;\n"
	"varying vec2 vv2_pos;\n"
	"void main() {\n"
		"vv2_pos = av2_pos;\n"
		"gl_Position = vec4(vv2_pos, 0., 1.);\n"
	"}\n";

static const char fsquad_fragment_src[] =
	"uniform sampler2D us2_tex;\n"
	"varying vec2 vv2_pos;\n"
	"void main() {\n"
		"gl_FragColor = texture2D(us2_tex, vv2_pos * .5 + .5);\n"
	"}\n";

static struct {
	AGLDrawSource source;
	AGLDrawMerge merge;
	AGLProgramUniform uniforms[2];
	AGLAttribute attrib_pos;
} fsquad;

static void fsquadInit() {
	fsquad.merge.depth.mode = AGLDM_Disabled;
	fsquad.merge.blend.enable = 0;

	fsquad.source.program = aGLProgramCreateSimple(fsquad_vertex_src, fsquad_fragment_src);
	if (fsquad.source.program < 1) {
		aAppDebugPrintf("Shader compilation error: %s", a_gl_error);
		aAppTerminate(-1);
	}

	fsquad.source.primitive.mode = GL_TRIANGLE_STRIP;
	fsquad.source.primitive.cull_mode = AGLCM_Disable;
	fsquad.source.primitive.front_face = AGLFF_CounterClockwise;
	fsquad.source.primitive.first = 0;
	fsquad.source.primitive.index.buffer = 0;
	fsquad.source.primitive.index.type = 0;
	fsquad.source.primitive.index.data.ptr = 0;
	fsquad.source.primitive.count = 4;

	fsquad.source.uniforms.p = fsquad.uniforms;
	fsquad.source.uniforms.n = COUNTOF(fsquad.uniforms);
	fsquad.source.attribs.p = &fsquad.attrib_pos;
	fsquad.source.attribs.n = 1;

	fsquad.attrib_pos.name = "av2_pos";
	fsquad.attrib_pos.size = 2;
	fsquad.attrib_pos.type = GL_FLOAT;
	fsquad.attrib_pos.normalized = GL_FALSE;
	fsquad.attrib_pos.stride = 2 * sizeof(float);
	fsquad.attrib_pos.ptr = fsquad_vertices;
	fsquad.attrib_pos.buffer = 0;

	fsquad.uniforms[0].name = "uf_aspect";
	fsquad.uniforms[0].type = AGLAT_Float;
	fsquad.uniforms[0].count = 1;

	fsquad.uniforms[1].name = "us2_tex";
	fsquad.uniforms[1].type = AGLAT_Texture;
	fsquad.uniforms[1].count = 1;
}

static void fsquadDraw(float aspect, const AGLTexture *tex, const AGLDrawTarget *target) {
	fsquad.uniforms[0].value.pf = &aspect;
	fsquad.uniforms[1].value.texture = tex;
	aGLDraw(&fsquad.source, &fsquad.merge, target);
}

static const float aabb_draw_vertices[] = {
	0.f, 0.f, 0.f,
	0.f, 0.f, 1.f,
	0.f, 1.f, 1.f,
	0.f, 1.f, 0.f,
	1.f, 0.f, 0.f,
	1.f, 0.f, 1.f,
	1.f, 1.f, 1.f,
	1.f, 1.f, 0.f
};

static const uint16_t aabb_draw_indices[] = {
	0, 1, 1, 2, 2, 3, 3, 0,
	4, 5, 5, 6, 6, 7, 7, 4,
	0, 4, 1, 5, 2, 6, 3, 7
};

static const char aabb_draw_vertex_src[] =
	"attribute vec3 av3_pos;\n"
	"uniform mat4 um4_MVP;\n"
	"uniform vec3 uv3_mul, uv3_add;\n"
	"varying vec3 vv3_pos;\n"
	"void main() {\n"
		"vv3_pos = av3_pos;\n"
		"gl_Position = um4_MVP * vec4(av3_pos * uv3_mul + uv3_add, 1.);\n"
	"}\n";

static const char aabb_draw_fragment_src[] =
	"uniform vec3 uv3_color;\n"
	"varying vec3 vv3_pos;\n"
	"void main() {\n"
		"gl_FragColor = vec4(vv3_pos * uv3_color, 1.);\n"
	"}\n";

enum { AABBDUniformMVP, AABBDUniformMul, AABBDUniformAdd, AABBDUniformColor, AABBDUniform_COUNT };

struct AABBDraw {
	struct AVec3f min, max, color;
	const struct AMat4f *mvp;
	const AGLDrawTarget *target;
};

static struct {
	AGLProgram program;
} aabb_draw;

static void aabbDraw(const struct AABBDraw *draw) {
	AGLDrawSource source;
	AGLDrawMerge merge;
	AGLProgramUniform uniforms[AABBDUniform_COUNT];
	AGLAttribute attrib_pos;
	const struct AVec3f mul = aVec3fSub(draw->max, draw->min);

	merge.depth.mode = AGLDM_Disabled;
	merge.blend.enable = 0;

	if (aabb_draw.program < 1) {
		aabb_draw.program = aGLProgramCreateSimple(aabb_draw_vertex_src, aabb_draw_fragment_src);
		if (aabb_draw.program < 1) {
			aAppDebugPrintf("Shader compilation error: %s", a_gl_error);
			aAppTerminate(-1);
		}
	}
	source.program = aabb_draw.program;

	source.primitive.mode = GL_LINES;
	source.primitive.cull_mode = AGLCM_Back;//AGLCM_Disable;
	source.primitive.front_face = AGLFF_CounterClockwise;
	source.primitive.first = 0;
	source.primitive.index.buffer = 0;
#if 1
	source.primitive.index.type = GL_UNSIGNED_SHORT;
	source.primitive.index.data.ptr = aabb_draw_indices;
	source.primitive.count = COUNTOF(aabb_draw_indices);
#else
	source.primitive.index.type = 0;//GL_UNSIGNED_SHORT;
	source.primitive.index.data.ptr = 0;//aabb_draw_indices;
	source.primitive.count = 8;(void)(aabb_draw_indices);//COUNTOF(aabb_draw_indices);
#endif

	source.uniforms.p = uniforms;
	source.uniforms.n = AABBDUniform_COUNT;
	source.attribs.p = &attrib_pos;
	source.attribs.n = 1;

	attrib_pos.name = "av3_pos";
	attrib_pos.size = 3;
	attrib_pos.type = GL_FLOAT;
	attrib_pos.normalized = GL_FALSE;
	attrib_pos.stride = 3 * sizeof(float);
	attrib_pos.ptr = aabb_draw_vertices;
	attrib_pos.buffer = 0;

	uniforms[AABBDUniformMVP].name = "um4_MVP";
	uniforms[AABBDUniformMVP].type = AGLAT_Mat4;
	uniforms[AABBDUniformMVP].count = 1;
	uniforms[AABBDUniformMVP].value.pf = &draw->mvp->X.x;

	uniforms[AABBDUniformMul].name = "uv3_mul";
	uniforms[AABBDUniformMul].type = AGLAT_Vec3;
	uniforms[AABBDUniformMul].count = 1;
	uniforms[AABBDUniformMul].value.pf = &mul.x;

	uniforms[AABBDUniformAdd].name = "uv3_add";
	uniforms[AABBDUniformAdd].type = AGLAT_Vec3;
	uniforms[AABBDUniformAdd].count = 1;
	uniforms[AABBDUniformAdd].value.pf = &draw->min.x;

	uniforms[AABBDUniformColor].name = "uv3_color";
	uniforms[AABBDUniformColor].type = AGLAT_Vec3;
	uniforms[AABBDUniformColor].count = 1;
	uniforms[AABBDUniformColor].value.pf = &draw->color.x;

	aGLDraw(&source, &merge, draw->target);
}
#endif

static struct {
	struct SimpleCamera camera;
	int forward, right, run;
	struct AVec3f center;
	float R;
	float lmn;

#if 0
	AGLDrawTarget screen;
	AGLDrawSource source;
	AGLDrawMerge merge;
	AGLAttribute attribs[Attrib_COUNT];
	AGLProgramUniform uniforms[Uniform_COUNT];
#endif
	struct BSPModel worldspawn;
	struct AMat4f model;
} g;

static void opensrcInit(struct ICollection *collection, const char *map) {
	cacheInit(&stack_persistent);

	if (!renderInit()) {
		PRINT("Failed to initialize render");
		aAppTerminate(-1);
	}


#if 0
	g.source.program = aGLProgramCreateSimple(vertex_src, fragment_src);
	if (g.source.program < 1) {
		aAppDebugPrintf("Shader compilation error: %s", a_gl_error);
		aAppTerminate(-1);
	}
	fsquadInit();

	{
		Texture tex_placeholder;
		const uint16_t pixels[] = {
			0xf81f, 0x07e0, 0x07e0, 0xf81f
		};
		tex_placeholder.gltex = aGLTextureCreate();
		AGLTextureUploadData up = {
			.width = 2,
			.height = 2,
			.format = AGLTF_U565_RGB,
			.pixels = pixels
		};
		aGLTextureUpload(&tex_placeholder.gltex, &up);
		cachePutTexture("opensource/placeholder", &tex_placeholder);
	}
#endif

	struct BSPLoadModelContext loadctx = {
		.collection = collection,
		.persistent = &stack_persistent,
		.tmp = &stack_temp,
		.model = &g.worldspawn
	};
	const enum BSPLoadResult result = bspLoadWorldspawn(loadctx, map);
	if (result != BSPLoadResult_Success) {
		aAppDebugPrintf("Cannot load map \"%s\": %d", map, result);
		aAppTerminate(-2);
	}

	aAppDebugPrintf("Loaded %s to %u draw calls", map, g.worldspawn.draws_count);
	aAppDebugPrintf("AABB (%f, %f, %f) - (%f, %f, %f)",
			g.worldspawn.aabb.min.x,
			g.worldspawn.aabb.min.y,
			g.worldspawn.aabb.min.z,
			g.worldspawn.aabb.max.x,
			g.worldspawn.aabb.max.y,
			g.worldspawn.aabb.max.z);

	g.center = aVec3fMulf(aVec3fAdd(g.worldspawn.aabb.min, g.worldspawn.aabb.max), .5f);
	g.R = aVec3fLength(aVec3fSub(g.worldspawn.aabb.max, g.worldspawn.aabb.min)) * .5f;

	aAppDebugPrintf("Center %f, %f, %f, R~=%f", g.center.x, g.center.y, g.center.z, g.R);

	g.model = aMat4fIdentity();

#if 0
	g.source.primitive.mode = GL_TRIANGLES;
	g.source.primitive.first = 0;
	g.source.primitive.cull_mode = AGLCM_Back;
	g.source.primitive.front_face = AGLFF_CounterClockwise;
	g.source.primitive.index.type = GL_UNSIGNED_SHORT;

	g.source.uniforms.p = g.uniforms;
	g.source.uniforms.n = Uniform_COUNT;
	g.source.attribs.p = g.attribs;
	g.source.attribs.n = Attrib_COUNT;

	g.attribs[AttribPos].name = "av3_pos";
	g.attribs[AttribPos].size = 3;
	g.attribs[AttribPos].type = GL_FLOAT;
	g.attribs[AttribPos].normalized = GL_FALSE;
	g.attribs[AttribPos].stride = sizeof(struct BSPModelVertex);
	g.attribs[AttribPos].ptr = 0;

	g.attribs[AttribNormal].name = "av3_normal";
	g.attribs[AttribNormal].size = 3;
	g.attribs[AttribNormal].type = GL_FLOAT;
	g.attribs[AttribNormal].normalized = GL_FALSE;
	g.attribs[AttribNormal].stride = sizeof(struct BSPModelVertex);
	g.attribs[AttribNormal].ptr = (void*)offsetof(struct BSPModelVertex, normal);

	g.attribs[AttribLightmapUV].name = "av2_lightmap";
	g.attribs[AttribLightmapUV].size = 2;
	g.attribs[AttribLightmapUV].type = GL_FLOAT;
	g.attribs[AttribLightmapUV].normalized = GL_FALSE;
	g.attribs[AttribLightmapUV].stride = sizeof(struct BSPModelVertex);
	g.attribs[AttribLightmapUV].ptr = (void*)offsetof(struct BSPModelVertex, lightmap_uv);

	g.attribs[AttribTextureUV].name = "av2_base0_uv";
	g.attribs[AttribTextureUV].size = 2;
	g.attribs[AttribTextureUV].type = GL_FLOAT;
	g.attribs[AttribTextureUV].normalized = GL_FALSE;
	g.attribs[AttribTextureUV].stride = sizeof(struct BSPModelVertex);
	g.attribs[AttribTextureUV].ptr = (void*)offsetof(struct BSPModelVertex, base0_uv);

	g.uniforms[UniformM].name = "um4_M";
	g.uniforms[UniformM].type = AGLAT_Mat4;
	g.uniforms[UniformM].count = 1;
	g.uniforms[UniformM].value.pf = &g.model.X.x;

	g.uniforms[UniformVP].name = "um4_VP";
	g.uniforms[UniformVP].type = AGLAT_Mat4;
	g.uniforms[UniformVP].count = 1;
	g.uniforms[UniformVP].value.pf = &g.camera.view_projection.X.x;

	g.uniforms[UniformLightmap].name = "us2_lightmap";
	g.uniforms[UniformLightmap].type = AGLAT_Texture;
	g.uniforms[UniformLightmap].count = 1;

	g.uniforms[UniformLightmapSize].name = "uv2_lightmap_size";
	g.uniforms[UniformLightmapSize].type = AGLAT_Vec2;
	g.uniforms[UniformLightmapSize].count = 1;

	g.uniforms[UniformTextureBase0].name = "tex_base0";
	g.uniforms[UniformTextureBase0].type = AGLAT_Texture;
	g.uniforms[UniformTextureBase0].count = 1;

	g.uniforms[UniformTextureBase0Size].name = "tex_base0_size";
	g.uniforms[UniformTextureBase0Size].type = AGLAT_Vec2;
	g.uniforms[UniformTextureBase0Size].count = 1;

	g.uniforms[UniformTextureBase1].name = "tex_base1";
	g.uniforms[UniformTextureBase1].type = AGLAT_Texture;
	g.uniforms[UniformTextureBase1].count = 1;

	g.uniforms[UniformTextureBase1Size].name = "tex_base1_size";
	g.uniforms[UniformTextureBase1Size].type = AGLAT_Vec2;
	g.uniforms[UniformTextureBase1Size].count = 1;

	g.uniforms[UniformLMN].name = "uf_lmn";
	g.uniforms[UniformLMN].type = AGLAT_Float;
	g.uniforms[UniformLMN].count = 1;
	g.uniforms[UniformLMN].value.pf = &g.lmn;

	aGLUniformLocate(g.source.program, g.uniforms, Uniform_COUNT);
	aGLAttributeLocate(g.source.program, g.attribs, Attrib_COUNT);

	g.merge.blend.enable = 0;
	g.merge.depth.mode = AGLDM_TestAndWrite;
	g.merge.depth.func = AGLDF_Less;

	g.screen.framebuffer = 0;

	g.lmn = 0;
#endif

	const float t = 0;
	simplecamLookAt(&g.camera,
			aVec3fAdd(g.center, aVec3fMulf(aVec3f(cosf(t*.5), sinf(t*.5), .25f), g.R*.5f)),
			g.center, aVec3f(0.f, 0.f, 1.f));
}

#if 0
static void drawBSPDraw(const struct BSPDraw *draw) {
	const ATimeUs start = aAppTime();
	g.source.primitive.index.data.offset = sizeof(uint16_t) * draw->start;
	g.source.primitive.count = draw->count;
	g.attribs[AttribPos].buffer =
		g.attribs[AttribNormal].buffer =
		g.attribs[AttribTextureUV].buffer =
		g.attribs[AttribLightmapUV].buffer = &draw->vbo;

	struct AVec2f tex0_size = aVec2f(1.f, 1.f);
	struct AVec2f tex1_size = aVec2f(1.f, 1.f);
	
	const struct Texture *texture = draw->material->base_texture[0];
	if (!texture)
		texture = cacheGetTexture("opensource/placeholder");

	g.uniforms[UniformTextureBase0Size].value.pf = &tex0_size.x;
	tex0_size = aVec2f(texture->gltex.width, texture->gltex.height);
	g.uniforms[UniformTextureBase0].value.texture = &texture->gltex;

	texture = draw->material->base_texture[1];
	if (!texture)
		texture = cacheGetTexture("opensource/placeholder");
	tex1_size = aVec2f(texture->gltex.width, texture->gltex.height);
	g.uniforms[UniformTextureBase1Size].value.pf = &tex1_size.x;
	g.uniforms[UniformTextureBase1].value.texture = &texture->gltex;

	aGLDraw(&g.source, &g.merge, &g.screen);
	profileEvent("drawBSDDraw", aAppTime() - start);
}

static void drawModel(const struct BSPModel *model) {
	const ATimeUs start = aAppTime();
	const struct AVec2f lm_size = aVec2f(model->lightmap.width, model->lightmap.height);
	g.uniforms[UniformLightmap].value.texture = &model->lightmap;
	g.uniforms[UniformLightmapSize].value.pf = &lm_size.x;
	g.source.primitive.index.buffer = &model->ibo;

	for (int i = 0; i < model->draws_count /*&& i < 200*/; ++i)
		drawBSPDraw(model->draws + i);
	profileEvent("drawModel", aAppTime() - start);
}
#endif

static void opensrcResize(ATimeUs timestamp, unsigned int old_w, unsigned int old_h) {
	(void)(timestamp); (void)(old_w); (void)(old_h);
#if 0
	g.screen.viewport.x = g.screen.viewport.y = 0;
	g.screen.viewport.w = a_app_state->width;
	g.screen.viewport.h = a_app_state->height;
#endif
	glViewport(0, 0, a_app_state->width, a_app_state->height);

	simplecamProjection(&g.camera, 1.f, g.R * 10.f, 3.1415926f/2.f, (float)a_app_state->width / (float)a_app_state->height);
	simplecamRecalc(&g.camera);
}

static void opensrcPaint(ATimeUs timestamp, float dt) {
	(void)(timestamp); (void)(dt);

	if (0) {
		const float t = timestamp * 1e-6f;
		simplecamLookAt(&g.camera,
				aVec3fAdd(g.center, aVec3fMulf(aVec3f(cosf(t*.5), sinf(t*.5), .25f), g.R*.5f)),
				g.center, aVec3f(0.f, 0.f, 1.f));
	} else {
		float move = dt * (g.run?3000.f:300.f);
		simplecamMove(&g.camera, aVec3f(g.right * move, 0.f, -g.forward * move));
		simplecamRecalc(&g.camera);
	}

#if 0
	AGLClearParams clear;
	clear.r = clear.g = clear.b = 0;
	clear.depth = 1.f;
	clear.bits = AGLCB_ColorAndDepth;
	aGLClear(&clear, &g.screen);

	if (0)
		fsquadDraw(1., &g.worldspawn.lightmap, &g.screen);

	drawModel(&g.worldspawn);

	if (0) {
		const struct AABBDraw aabb = {
			.min = g.worldspawn.aabb.min,
			.max = g.worldspawn.aabb.max,
			.color = aVec3ff(1),
			.mvp = &g.camera.view_projection,
			.target = &g.screen
		};
		aabbDraw(&aabb);
	}
#endif

	renderClear();

	renderModelDraw(&g.camera.view_projection, g.lmn, &g.worldspawn);

	if (profilerFrame()) {
		int triangles = 0;
		for (int i = 0; i < g.worldspawn.draws_count; ++i) {
			triangles += g.worldspawn.draws[i].count / 3;
		}
		PRINTF("Total triangles: %d", triangles);
	}
}

static void opensrcKeyPress(ATimeUs timestamp, AKey key, int pressed) {
	(void)(timestamp); (void)(key); (void)(pressed);
	//printf("KEY %u %d %d\n", timestamp, key, pressed);
	switch (key) {
	case AK_Esc:
		if (!pressed) break;
		if (a_app_state->grabbed)
			aAppGrabInput(0);
		else
			aAppTerminate(0);
		break;
	case AK_W: g.forward += pressed?1:-1; break;
	case AK_S: g.forward += pressed?-1:1; break;
	case AK_A: g.right += pressed?-1:1; break;
	case AK_D: g.right += pressed?1:-1; break;
	case AK_LeftShift: g.run = pressed; break;
	case AK_E: g.lmn = pressed; break;
	default: break;
	}
}

static void opensrcPointer(ATimeUs timestamp, int dx, int dy, unsigned int btndiff) {
	(void)(timestamp); (void)(dx); (void)(dy); (void)(btndiff);
	//printf("PTR %u %d %d %x\n", timestamp, dx, dy, btndiff);
	if (a_app_state->grabbed) {
		simplecamRotatePitch(&g.camera, dy * -4e-3f);
		simplecamRotateYaw(&g.camera, dx * -4e-3f);
		simplecamRecalc(&g.camera);
	} else if (btndiff)
		aAppGrabInput(1);
}

void attoAppInit(struct AAppProctable *proctable) {
	profilerInit();
	//aGLInit();
	const int max_collections = 16;
	struct FilesystemCollection collections[max_collections];
	int free_collection = 0;
	const char *map = 0;

	for (int i = 1; i < a_app_state->argc; ++i) {
		const char *argv = a_app_state->argv[i];
		if (strcmp(argv, "-d") == 0) {
			if (i == a_app_state->argc - 1) {
				aAppDebugPrintf("-d requires an argument");
				goto print_usage_and_exit;
			}
			const char *value = a_app_state->argv[++i];

			if (free_collection >= max_collections) {
				aAppDebugPrintf("Too many fs collections specified: %s", value);
				goto print_usage_and_exit;
			}

			filesystemCollectionCreate(collections + (free_collection++), value);
		} else {
			if (map) {
				aAppDebugPrintf("Only one map can be specified");
				goto print_usage_and_exit;
			}
			map = argv;
		}
	}

	if (!map || !free_collection) {
		aAppDebugPrintf("At least one map and one collection required");
		goto print_usage_and_exit;
	}

	for (int i = 0; i < free_collection - 1; ++i)
		collections[i].head.next = &collections[i+1].head;

	opensrcInit(&collections[0].head, map);

	proctable->resize = opensrcResize;
	proctable->paint = opensrcPaint;
	proctable->key = opensrcKeyPress;
	proctable->pointer = opensrcPointer;
	return;

print_usage_and_exit:
	aAppDebugPrintf("usage: %s <-d path> ... mapname", a_app_state->argv[0]);
	aAppTerminate(1);
}
