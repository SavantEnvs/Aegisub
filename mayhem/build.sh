#!/usr/bin/env bash
#
# mayhem/build.sh — build Aegisub's fuzz harnesses and its upstream gtest suite.
#
# Upstream's meson build unconditionally needs wxWidgets, libass, fontconfig and a LuaJIT
# subproject for the whole GUI app. This compiles what the fuzzed paths need directly, with the
# same defines meson would generate (mayhem/harnesses/acconf_fuzz.h): the static library
# libaegisub/ (minus lua/, the automation runtime), the GUI-free parsers MatroskaParser.c and
# video_provider_yuv4mpeg.cpp, and the subtitle readers (ass_*, subtitle_format_*, mkv_wrap)
# against the apt wxWidgets 3.2.
#
#   pass 1: libaegisub + src parsers + harnesses with $SANITIZER_FLAGS $DEBUG_FLAGS
#           -> mayhem/out/<fuzzer> (libFuzzer) and mayhem/out/<fuzzer>-standalone
#   pass 2: libaegisub + tests/ (upstream gtest suite) with normal flags -> mayhem/out/gtest-main
#
# Everything is rebuilt from scratch each run (idempotent), all inputs come from the checkout
# and apt packages baked into the image (air-gapped).
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${AR:=ar}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
M="$SRC/mayhem"
OUT="$M/out"
FZ="$M/build-fuzz"
TB="$M/build-test"
rm -rf "$FZ" "$TB" "$OUT"
mkdir -p "$FZ" "$TB" "$OUT"

LIB_SRCS=$(ls libaegisub/ass/*.cpp libaegisub/audio/*.cpp libaegisub/common/*.cpp \
              libaegisub/common/cajun/*.cpp libaegisub/unix/*.cpp)
COMMON_CXX="-std=c++20 -include $M/harnesses/acconf_fuzz.h -I$SRC/libaegisub/include -I$SRC/libaegisub"
LIBS="-lboost_locale -lboost_thread -lboost_chrono -lboost_regex -licui18n -licuuc -licudata -lz -lpthread"

# compile <outdir> <compiler> <flags> <src...>: one object per source, in parallel.
compile() {
	local dir="$1" comp="$2" flags="$3"; shift 3
	printf '%s\n' "$@" | xargs -P "$MAYHEM_JOBS" -I{} sh -c \
		'o="$0/$(echo "$3" | tr / _).o"; $1 $2 -c "$3" -o "$o"' "$dir" "$comp" "$flags" {}
}

# ---- pass 1: sanitized fuzz build ----------------------------------------------------------
FFLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link"
mkdir -p "$FZ/lib"
# shellcheck disable=SC2086
compile "$FZ/lib" "$CXX" "$FFLAGS $COMMON_CXX" $LIB_SRCS
rm -f "$FZ/libaegisub.a"
"$AR" rcs "$FZ/libaegisub.a" "$FZ"/lib/*.o

$CC $FFLAGS -I"$SRC/src" -c src/MatroskaParser.c -o "$FZ/MatroskaParser.o"
$CXX $FFLAGS $COMMON_CXX -I"$M/shim" -I"$SRC/src" -c src/video_provider_yuv4mpeg.cpp -o "$FZ/y4m.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$M/lsan_off.cc" -o "$FZ/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$FZ/standalone_main.o"

link_fuzzer() { # name objects...
	local name="$1"; shift
	$CXX $FFLAGS $LIB_FUZZING_ENGINE "$@" "$FZ/lsan_off.o" "$FZ/libaegisub.a" $LIBS -o "$OUT/$name"
	$CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$FZ/standalone_main.o" "$@" "$FZ/lsan_off.o" "$FZ/libaegisub.a" $LIBS \
		-o "$OUT/$name-standalone"
}

$CC $FFLAGS -I"$SRC/src" -c "$M/harnesses/fuzz_matroska.c" -o "$FZ/fuzz_matroska.o"
link_fuzzer fuzz_matroska "$FZ/fuzz_matroska.o" "$FZ/MatroskaParser.o"

$CXX $FFLAGS $COMMON_CXX -I"$SRC/src" -c "$M/harnesses/fuzz_yuv4mpeg.cpp" -o "$FZ/fuzz_yuv4mpeg.o"
link_fuzzer fuzz_yuv4mpeg "$FZ/fuzz_yuv4mpeg.o" "$FZ/y4m.o"

# Subtitle readers from src/ (ASS/SSA, SRT, MicroDVD, TXT, TTXT, Matroska via mkv_wrap). These
# use wxWidgets (wxString, wx XML, translations), so they build against the apt wx 3.2 headers;
# no window is ever created (mayhem/harnesses/headless_ui.cpp answers the modal prompts).
WX_CXXFLAGS=$(wx-config --cxxflags)
WX_LIBS=$(wx-config --libs core,base,xml)
SUB_SRCS="src/ass_file.cpp src/ass_style.cpp src/ass_style_storage.cpp src/ass_parser.cpp
	src/ass_dialogue.cpp src/ass_entry.cpp src/ass_attachment.cpp src/ass_karaoke.cpp
	src/subtitle_format.cpp src/subtitle_format_ass.cpp src/subtitle_format_srt.cpp
	src/subtitle_format_microdvd.cpp src/subtitle_format_txt.cpp src/subtitle_format_ssa.cpp
	src/subtitle_format_ttxt.cpp src/subtitle_format_ebu3264.cpp src/subtitle_format_encore.cpp
	src/subtitle_format_transtation.cpp src/mkv_wrap.cpp src/text_file_reader.cpp
	src/text_file_writer.cpp src/ass_override.cpp src/compat.cpp src/version.cpp
	src/dialog_export_ebu3264.cpp src/utils.cpp $M/harnesses/headless_ui.cpp"
SUB_CXX="$COMMON_CXX $WX_CXXFLAGS -I$SRC/src -I$SRC/src/include -I$M/harnesses -I$FZ/gen"
# upstream's default_config.json as a byte array, as meson's respack embeds it into the app
mkdir -p "$FZ/gen" "$FZ/sub"
{
	echo "static const unsigned char default_config_json[] = {"
	od -An -v -tx1 src/libresrc/default_config.json | sed -e 's/ *\([0-9a-f][0-9a-f]\)/0x\1,/g'
	echo "};"
	echo "static const unsigned int default_config_json_len = $(wc -c < src/libresrc/default_config.json);"
} > "$FZ/gen/default_config_json.h"
# what tools/version.sh writes for an untagged tree (version.cpp only formats it for the writers)
printf '#define BUILD_GIT_VERSION_NUMBER 0\n#define BUILD_GIT_VERSION_STRING "mayhem"\n#define TAGGED_RELEASE 0\n#define INSTALLER_VERSION "0.0.0"\n#define RESOURCE_BASE_VERSION 0, 0, 0\n' \
	> "$FZ/gen/git_version.h"
# shellcheck disable=SC2086
compile "$FZ/sub" "$CXX" "$FFLAGS $SUB_CXX" $SUB_SRCS
for h in subtitles mkv_subs; do
	$CXX $FFLAGS $SUB_CXX -c "$M/harnesses/fuzz_$h.cpp" -o "$FZ/fuzz_$h.o"
	LIBS="$WX_LIBS $LIBS" link_fuzzer "fuzz_$h" "$FZ/fuzz_$h.o" "$FZ"/sub/*.o "$FZ/MatroskaParser.o"
done

for h in vfr keyframe pcm_audio json dialogue ass_fields; do
	$CXX $FFLAGS $COMMON_CXX -c "$M/harnesses/fuzz_$h.cpp" -o "$FZ/fuzz_$h.o"
	link_fuzzer "fuzz_$h" "$FZ/fuzz_$h.o"
done

# ---- pass 2: upstream gtest suite, normal flags --------------------------------------------
TFLAGS="-O1 $COVERAGE_FLAGS"
mkdir -p "$TB/lib" "$TB/t"
# shellcheck disable=SC2086
compile "$TB/lib" "$CXX" "$TFLAGS $COMMON_CXX" $LIB_SRCS
# Every test in tests/meson.build except lua_lfs.cpp (needs the LuaJIT runtime) and
# language_match.cpp (tests src/language_match.h, which needs wxWidgets); neither is built here.
TEST_SRCS="tests/support/main.cpp tests/support/util.cpp $(ls tests/tests/*.cpp | grep -v -e lua_lfs.cpp -e language_match.cpp)"
# shellcheck disable=SC2086
compile "$TB/t" "$CXX" "$TFLAGS $COMMON_CXX -I$SRC/tests/support" $TEST_SRCS
$CXX $TFLAGS "$TB"/t/*.o "$TB"/lib/*.o -lgmock -lgtest $LIBS -o "$OUT/gtest-main"

ls -l "$OUT"
