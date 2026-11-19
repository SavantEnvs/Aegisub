// JSON config reader: agi::json_util::parse (cajun) — the parser behind config.json, hotkey.json
// and mru.json — then agi::Options::LoadConfig on the parsed tree.
#include "fuzz_common.h"
#include <libaegisub/cajun/elements.h>
#include <libaegisub/exception.h>
#include <libaegisub/json.h>
#include <libaegisub/option.h>

#include <sstream>

static const char default_config[] =
    R"({"Bool":true,"Int":5,"Double":1.5,"String":"x","Color":"#112233",)"
    R"("Arr":{"Bool":[{"bool":true}],"Int":[{"int":1}],"Double":[{"double":1.0}],)"
    R"("String":[{"string":"a"}],"Color":[{"color":"#000000"}]},"Group":{"Sub":1}})";

extern "C" int LLVMFuzzerInitialize(int *, char ***) { fuzz::init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	std::string in(reinterpret_cast<const char *>(data), size);
	try {
		std::istringstream s(in);
		json::UnknownElement e = agi::json_util::parse(s);
		(void)e;
	} catch (agi::Exception const&) {
	} catch (json::Exception const&) {
	} catch (std::exception const&) { // e.g. std::out_of_range from number parsing: the app reports these as errors (src/main.cpp)
	}
	try {
		agi::Options opt("", default_config, agi::Options::FLUSH_SKIP);
		std::istringstream s(in);
		opt.ConfigNext(s);
	} catch (agi::Exception const&) {
	} catch (json::Exception const&) {
	} catch (std::exception const&) { // e.g. std::out_of_range from number parsing: the app reports these as errors (src/main.cpp)
	}
	return 0;
}
