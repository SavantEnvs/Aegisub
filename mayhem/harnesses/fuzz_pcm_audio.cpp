// RIFF WAV / Wave64 loader: agi::CreatePCMAudioProvider over a file mapping, then the sample
// format conversion chain the audio display reads through.
#include "fuzz_common.h"
#include <libaegisub/audio/provider.h>
#include <libaegisub/exception.h>

extern "C" int LLVMFuzzerInitialize(int *, char ***) { fuzz::init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	fuzz::TempFile f(data, size, ".wav");
	try {
		auto p = agi::CreatePCMAudioProvider(f.path, nullptr);
		if (!p) return 0;
		auto conv = agi::CreateConvertAudioProvider(std::move(p));
		int64_t n = conv->GetNumSamples();
		int bpf = conv->GetBytesPerSample() * conv->GetChannels();
		if (bpf <= 0 || bpf > 64) return 0;
		std::vector<char> buf(256 * (size_t)bpf);
		for (int64_t start : {(int64_t)-16, (int64_t)0, n / 2, n - 16, n})
			conv->GetAudio(buf.data(), start, 256);
		conv->GetAudioWithVolume(buf.data(), 0, 256, 0.5);
	} catch (agi::Exception const&) {
	} catch (std::exception const&) { // e.g. std::out_of_range from number parsing: the app reports these as errors (src/main.cpp)
	}
	return 0;
}
