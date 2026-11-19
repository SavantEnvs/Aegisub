// YUV4MPEG2 video loader: CreateYUV4MPEGVideoProvider (header/frame-header parsing and frame
// indexing), then GetFrame on the first and last frame (YCbCr -> BGRA conversion).
// Upstream fix 0b228ef985 (validate frame sizes) is on this path.
#include "fuzz_common.h"
#include "include/aegisub/video_provider.h"
#include "video_frame.h"
#include <libaegisub/exception.h>
#include <libaegisub/background_runner.h>
#include <libaegisub/ycbcr.h>

std::unique_ptr<VideoProvider> CreateYUV4MPEGVideoProvider(agi::fs::path const&, agi::ycbcr::Header, agi::BackgroundRunner *);

extern "C" int LLVMFuzzerInitialize(int *, char ***) { fuzz::init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	fuzz::TempFile f(data, size, ".y4m");
	try {
		auto p = CreateYUV4MPEGVideoProvider(f.path, agi::ycbcr::Header(agi::ycbcr::header_missing{}), nullptr);
		if (!p) return 0;
		int n = p->GetFrameCount();
		p->GetFPS();
		if (n > 0) {
			VideoFrame frame;
			p->GetFrame(0, frame);
			p->GetFrame(n - 1, frame);
		}
	} catch (agi::Exception const&) {
	} catch (std::exception const&) { // e.g. std::out_of_range from number parsing: the app reports these as errors (src/main.cpp)
	}
	return 0;
}
