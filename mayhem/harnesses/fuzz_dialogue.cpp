// ASS dialogue-body lexer run on every subtitle line: TokenizeDialogueBody, SplitWords,
// MarkDrawings and SyntaxHighlight (null spellchecker), as in tests/tests/syntax_highlight.cpp.
#include "fuzz_common.h"
#include <libaegisub/ass/dialogue_parser.h>

extern "C" int LLVMFuzzerInitialize(int *, char ***) { fuzz::init(); return 0; }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size == 0) return 0;
	bool templater = data[0] & 1;
	std::string_view text(reinterpret_cast<const char *>(data) + 1, size - 1);
	auto tokens = agi::ass::TokenizeDialogueBody(text, templater);
	agi::ass::SplitWords(text, tokens);
	auto hl = agi::ass::SyntaxHighlight(text, tokens, nullptr);
	auto tokens2 = agi::ass::TokenizeDialogueBody(text, templater);
	agi::ass::MarkDrawings(text, tokens2);
	(void)hl;
	return 0;
}
