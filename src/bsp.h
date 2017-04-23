#pragma once
#include "material.h"
#include "render.h"
#include "atto/math.h"

struct Material;

struct AABB { struct AVec3f min, max; };
struct Plane { struct AVec3f n; float d; };

struct BSPModelVertex {
	struct AVec3f vertex;
	struct AVec3f normal;
	struct AVec2f lightmap_uv;
	struct AVec2f tex_uv;
};

struct BSPDraw {
	const struct Material *material;
	unsigned int start, count;
};

/* TODO
struct BSPNode {
	struct AABB aabb;
	struct {
		struct BSPDraw *p;
		unsigned n;
	} draw;
	struct Plane plane;
	int left, right;
};
*/

struct BSPModel {
	struct AABB aabb;
	RTexture lightmap;
	RBuffer vbo, ibo;
	int draws_count;
	struct BSPDraw *draws;
};

struct ICollection;
struct MemoryPool;
struct Stack;

struct BSPLoadModelContext {
	struct ICollection *collection;
	struct Stack *persistent;
	struct Stack *tmp;

	/* allocated by caller, populated by callee */
	struct BSPModel *model;
};

enum BSPLoadResult {
	BSPLoadResult_Success,
	BSPLoadResult_ErrorFileOpen,
	BSPLoadResult_ErrorFileFormat,
	BSPLoadResult_ErrorMemory,
	BSPLoadResult_ErrorTempMemory,
	BSPLoadResult_ErrorCapabilities
};

enum BSPLoadResult bspLoadWorldspawn(struct BSPLoadModelContext context, const char *mapname);
