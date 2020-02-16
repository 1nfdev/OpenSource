cmake_minimum_required(VERSION 3.10)

project(OpenSource C)

set(ATTO_SOURCES
	src/atto/src/app_x11.c
	src/atto/src/app_linux.c
)

add_library(atto STATIC ${ATTO_SOURCES} ${ATTO_HEADERS})
target_include_directories(atto PUBLIC src/atto/include)
find_package(OpenGL REQUIRED)
find_package(X11 REQUIRED)
target_link_libraries(atto ${X11_LIBRARIES} ${X11_Xext_LIB} ${X11_Xfixes_LIB} OpenGL::GL)
target_include_directories(atto PRIVATE ${X11_INCLUDE_DIR})

set(SOURCES
	src/OpenSource.c
	src/bsp.c
	src/atlas.c
	src/filemap.c
	src/camera.c
	src/collection.c
	src/vmfparser.c
	src/material.c
	src/texture.c
	src/cache.c
	src/dxt.c
	src/render.c
	src/profiler.c
)

add_executable(OpenSource ${SOURCES} ${HEADERS})
target_link_libraries(OpenSource atto m)
