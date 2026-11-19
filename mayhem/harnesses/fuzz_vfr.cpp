// Timecode file (v1/v2) loader: agi::vfr::Framerate(path), then the frame<->time mapping the
// editor uses. Upstream fix 5d01c4f553 (zero denominator) lives on this path.
#include "fuzz_common.h"
#include <libaegisub/exception.h>
#include <libaegisub/vfr.h>

extern "C" int LLVMFuzzerInitialize(int *, char ***) { fuzz::init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	fuzz::TempFile f(data, size, ".txt");
	try {
		agi::vfr::Framerate fps(f.path);
		volatile double d = fps.FPS();
		(void)d;
		fps.IsVFR();
		int last = 100;
		int h, m, sec, fr;
		for (int frame : {0, 1, last / 2, last - 1, last, last + 5}) {
			int t = fps.TimeAtFrame(frame, agi::vfr::START);
			fps.FrameAtTime(t, agi::vfr::END);
			fps.TimeAtFrame(frame, agi::vfr::END);
			fps.TimeAtFrame(frame);
			fps.SmpteAtFrame(frame, &h, &m, &sec, &fr);
			fps.FrameAtSmpte(h, m, sec, fr);
		}
		for (int ms : {0, 1, 1000, 60000, 3600000})
			fps.FrameAtTime(ms), fps.FrameAtTime(ms, agi::vfr::START);
	} catch (agi::Exception const&) {
	} catch (std::exception const&) { // e.g. std::out_of_range from number parsing: the app reports these as errors (src/main.cpp)
	}
	return 0;
}
