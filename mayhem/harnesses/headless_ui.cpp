// Headless stand-ins for the three modal GUI prompts the subtitle readers can raise. A fuzzer has
// no display, so the real wx dialogs cannot be shown; each stand-in answers the way a user who
// accepts the default would. None of them replaces parsing code: they sit where the app waits
// for the user, and the readers' own code runs unchanged on either side of them.
//   - wxGetSingleChoiceIndex: AskForFPS (MicroDVD without an fps line) and the MKV track picker
//     -> the dialog's initial selection.
//   - ShowPlainTextImportDialog (src/dialog_text_import.cpp, not linked): the TXT import options
//     dialog -> "OK" with the options as configured.
//   - DialogProgress (src/dialog_progress.cpp, not linked): the progress dialog mkv_wrap runs the
//     subtitle read under -> runs the task synchronously with a sink that never cancels.
#include "acconf_fuzz.h"
#include "dialog_progress.h"
#include "dialogs.h"

#include <wx/choicdlg.h>

int wxGetSingleChoiceIndex(const wxString &, const wxString &, const wxArrayString &choices,
                           wxWindow *, int, int, bool, int, int, int initialSelection) {
	return choices.empty() ? -1 : initialSelection;
}

bool ShowPlainTextImportDialog() { return true; }

namespace {
struct HeadlessSink final : agi::ProgressSink {
	void SetIndeterminate() override {}
	void SetTitle(std::string const &) override {}
	void SetMessage(std::string const &) override {}
	void SetProgress(int64_t, int64_t) override {}
	void Log(std::string const &) override {}
	bool IsCancelled() override { return false; }
};
}

DialogProgress::DialogProgress(wxWindow *, wxString const &, wxString const &) : ps(nullptr) {}

void DialogProgress::Run(std::function<void(agi::ProgressSink *)> task) {
	HeadlessSink sink;
	task(&sink);
}

// Only for the DialogProgress stand-in above: its wxDialog base is default-constructed with GTK
// never initialised, and the base destructors (~wxTopLevelWindowGTK/~wxWindowGTK) disconnect
// signals on objects that do not exist, which GLib reports as GLib-GObject-CRITICAL on every MKV
// input. That teardown never happens in the app (it always has a display), so drop just those
// messages. glib is already loaded through libwx_gtk3u_core; the image has no glib headers, so
// g_log_set_handler is looked up at runtime. Nothing in Aegisub's own code logs to this domain.
#include <dlfcn.h>
void fuzz_silence_headless_gobject() {
	using LogFunc = void (*)(const char *, int, const char *, void *);
	using SetHandler = unsigned (*)(const char *, int, LogFunc, void *);
	auto set = reinterpret_cast<SetHandler>(dlsym(RTLD_DEFAULT, "g_log_set_handler"));
	if (!set) return;
	constexpr int G_LOG_LEVEL_CRITICAL = 1 << 3;
	set("GLib-GObject", G_LOG_LEVEL_CRITICAL, [](const char *, int, const char *, void *) {}, nullptr);
}
