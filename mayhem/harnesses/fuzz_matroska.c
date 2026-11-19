// Matroska/EBML demuxer (src/MatroskaParser.c) over an in-memory InputStream modelled on
// MkvStdIO in src/mkv_wrap.cpp: open, walk tracks/attachments/chapters/tags, then read every
// frame and decompress (zlib / header-stripping) frames of compressed tracks as mkv_wrap does.
// Upstream fixes 7050497230 (integer underflows) and 3b165b4e91 (memcpy from NULL) are here.
#include <stddef.h>
#include <stdint.h>
#include "MatroskaParser.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	InputStream base;
	const uint8_t *data;
	uint64_t size;
} MemIO;

static int mem_read(InputStream *st, uint64_t pos, void *buffer, int count) {
	MemIO *m = (MemIO *)st;
	if (pos >= m->size) return 0;
	uint64_t remaining = m->size - pos;
	if (remaining < (uint64_t)count) count = (int)remaining;
	if (count <= 0) return 0;
	if (buffer) memcpy(buffer, m->data + pos, (size_t)count);
	return count;
}

static int64_t mem_scan(InputStream *st, uint64_t start, unsigned signature) {
	MemIO *m = (MemIO *)st;
	unsigned cmp = 0;
	for (uint64_t i = start; i < m->size; i++) {
		cmp = ((cmp << 8) | m->data[i]) & 0xffffffff;
		if (cmp == signature) return (int64_t)i - 4;
	}
	return -1;
}

static unsigned mem_cachesize(InputStream *st) { (void)st; return 16 * 1024 * 1024; }
static const char *mem_error(InputStream *st) { (void)st; return ""; }
static void *mem_alloc(InputStream *st, size_t size) { (void)st; return malloc(size); }
static void *mem_realloc(InputStream *st, void *p, size_t size) { (void)st; return realloc(p, size); }
static void mem_free(InputStream *st, void *p) { (void)st; free(p); }
static int mem_progress(InputStream *st, uint64_t cur, uint64_t max) { (void)st; (void)cur; (void)max; return 1; }
static int64_t mem_filesize(InputStream *st) { return (int64_t)((MemIO *)st)->size; }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	MemIO io;
	memset(&io, 0, sizeof io);
	io.base.read = mem_read;
	io.base.scan = mem_scan;
	io.base.getcachesize = mem_cachesize;
	io.base.geterror = mem_error;
	io.base.memalloc = mem_alloc;
	io.base.memrealloc = mem_realloc;
	io.base.memfree = mem_free;
	io.base.progress = mem_progress;
	io.base.getfilesize = mem_filesize;
	io.data = data;
	io.size = size;

	char err[2048];
	MatroskaFile *mf = mkv_OpenEx(&io.base, 0, 0, err, sizeof err);
	if (!mf) return 0;

	mkv_GetFileInfo(mf);
	unsigned ntracks = mkv_GetNumTracks(mf);
	CompressedStream *cs = NULL;
	unsigned cs_track = 0;
	for (unsigned t = 0; t < ntracks; t++) {
		TrackInfo *ti = mkv_GetTrackInfo(mf, t);
		if (ti && !cs && ti->CompEnabled) {
			cs = cs_Create(mf, t, err, sizeof err);
			cs_track = t;
		}
	}
	Attachment *at; unsigned nat;
	mkv_GetAttachments(mf, &at, &nat);
	Chapter *ch; unsigned nch;
	mkv_GetChapters(mf, &ch, &nch);
	Tag *tg; unsigned ntg;
	mkv_GetTags(mf, &tg, &ntg);

	unsigned track, frame_size, flags;
	uint64_t start, end, pos;
	char buf[4096];
	int frames = 0;
	while (frames++ < 10000 &&
	       mkv_ReadFrame(mf, 0, &track, &start, &end, &pos, &frame_size, &flags) == 0) {
		if (cs && track == cs_track) {
			cs_NextFrame(cs, pos, frame_size);
			int r, guard = 0;
			while ((r = cs_ReadData(cs, buf, sizeof buf)) > 0 && guard++ < 4096) {}
		}
	}
	mkv_Seek(mf, 0, MKVF_SEEK_TO_PREV_KEYFRAME);
	mkv_ReadFrame(mf, 0, &track, &start, &end, &pos, &frame_size, &flags);

	if (cs) cs_Destroy(cs);
	mkv_Close(mf);
	return 0;
}
