// Opening a Matroska file as subtitles (MKVSubtitleFormat -> MatroskaWrapper::GetSubtitles in
// src/mkv_wrap.cpp): track selection, CodecPrivate header, block read loop, zlib/header-strip
// decompression, SSA/SRT line rebuilding (upstream fix 3b165b4e91 is in this file).
#include "fuzz_subtitles.h"

extern "C" int LLVMFuzzerInitialize(int *, char ***) { fuzz::init_subtitles(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	fuzz::load_subtitles(data, size, ".mkv");
	return 0;
}
