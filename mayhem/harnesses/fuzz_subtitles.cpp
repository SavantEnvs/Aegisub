// All subtitle readers through SubtitleFormat::GetReader: the first byte picks the file
// extension (and so the reader), the rest is the file. ASS/SSA (ass_parser.cpp), SRT, MicroDVD,
// plain text, TTXT (wx XML) and Matroska (mkv_wrap.cpp).
#include "fuzz_subtitles.h"

extern "C" int LLVMFuzzerInitialize(int *, char ***) { fuzz::init_subtitles(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	static const char *const suffixes[] = {".ass", ".ssa", ".srt", ".sub", ".txt", ".ttxt", ".mkv", ".mks"};
	if (size < 1) return -1;
	fuzz::load_subtitles(data + 1, size - 1, suffixes[data[0] % (sizeof suffixes / sizeof *suffixes)]);
	return 0;
}
