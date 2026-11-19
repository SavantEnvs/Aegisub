// Shared helpers for the libaegisub harnesses: process init (as tests/support/main.cpp does) and
// staging the input as a scratch file, since the loaders take a path and memory-map it.
#pragma once
#include <libaegisub/dispatch.h>
#include <libaegisub/fs.h>
#include <libaegisub/log.h>
#include <libaegisub/util.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>
#include <string>
#include <unistd.h>

namespace fuzz {
// The log sink keeps at most 250 messages in a ring buffer; nothing in it is read back, so it
// does not make one input's result depend on another's.
inline void init() {
	static bool done = false;
	if (done) return;
	done = true;
	agi::dispatch::Init([](agi::dispatch::Thunk) {});
	agi::util::InitLocale();
	agi::log::log = new agi::log::LogSink;
}

struct TempFile {
	std::string path;
	TempFile(const uint8_t *data, size_t size, const char *suffix = "") {
		const char *dir = getenv("TMPDIR");
		if (!dir || !*dir) dir = "/tmp";
		std::string tmpl = std::string(dir) + "/aegifuzz-XXXXXX" + suffix;
		std::vector<char> buf(tmpl.begin(), tmpl.end());
		buf.push_back('\0');
		int fd = mkstemps(buf.data(), (int)strlen(suffix));
		if (fd < 0) abort();
		path = buf.data();
		size_t off = 0;
		while (off < size) {
			ssize_t n = write(fd, data + off, size - off);
			if (n <= 0) abort();
			off += (size_t)n;
		}
		close(fd);
	}
	~TempFile() { unlink(path.c_str()); }
};
}
