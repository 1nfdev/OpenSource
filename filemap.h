#ifndef FILEMAP_H__INCLUDED
#define FILEMAP_H__INCLUDED

#include <stddef.h> /* size_t */

struct AFileMap {
	const void *map;
	size_t size;
	struct {
		int fd;
	} impl_;
};

struct AFileMap aFileMapOpen(const char *filename);
void aFileMapClose(struct AFileMap *file);

struct AFile {
	size_t size;
	struct {
		int fd;
	} impl_;
};

#define AFileError ((size_t)-1)

enum AFileResult {
	AFile_Success,
	AFile_Fail
};

/* reset file to default state, useful for initialization */
void aFileReset(struct AFile *file);
enum AFileResult aFileOpen(struct AFile *file, const char *filename);
size_t aFileRead(struct AFile *file, size_t size, void *buffer);
size_t aFileReadAtOffset(struct AFile *file, size_t off, size_t size, void *buffer);
void aFileClose(struct AFile *file);

#endif /* ifndef FILEMAP_H__INCLUDED */
