// Field-level ASS decoders: attachment UUDecode (and round trip), timestamps (agi::Time),
// colours (agi::Color, boost.spirit parser), inline extradata strings.
#include "fuzz_common.h"
#include <libaegisub/ass/string_codec.h>
#include <libaegisub/ass/time.h>
#include <libaegisub/ass/uuencode.h>
#include <libaegisub/color.h>

extern "C" int LLVMFuzzerInitialize(int *, char ***) { fuzz::init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	std::string_view s(reinterpret_cast<const char *>(data), size);
	auto dec = agi::ass::UUDecode(std::span<const char>(s.data(), s.size()));
	std::string enc = agi::ass::UUEncode(dec, true);
	auto dec2 = agi::ass::UUDecode(enc);
	if (dec2 != dec) abort(); // UUEncode/UUDecode must round-trip
	agi::Time t(s);
	(void)t.GetAssFormatted(true);
	agi::Color c(s);
	(void)c.GetAssOverrideFormatted();
	(void)c.GetHexFormatted(true);
	std::string d = agi::ass::inline_string_decode(s);
	(void)agi::ass::inline_string_encode(d);
	return 0;
}
