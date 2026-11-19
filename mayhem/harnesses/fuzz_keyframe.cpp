// Keyframe file loader: agi::keyframe::Load sniffs and parses Aegisub, XviD, DivX, x264 and
// WWXD keyframe logs.
#include "fuzz_common.h"
#include <libaegisub/exception.h>
#include <libaegisub/keyframe.h>

extern "C" int LLVMFuzzerInitialize(int *, char ***) { fuzz::init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	fuzz::TempFile f(data, size, ".txt");
	try {
		auto kf = agi::keyframe::Load(f.path);
		(void)kf;
	} catch (agi::Exception const&) {
	} catch (std::exception const&) { // e.g. std::out_of_range from number parsing: the app reports these as errors (src/main.cpp)
	}
	return 0;
}
