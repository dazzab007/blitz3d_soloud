#!/bin/sh
# Run every tests/blitzcc/*.bb through blitzcc and compare stdout+stderr with
# the .expected file next to it. Optional header comments in the .bb:
#   ; flags: -d      extra blitzcc flags
#   ; rc: 1          expected exit status (default 0)
#   ; graphics       graphics mode (hidden window, headless); otherwise blitzcc -t
#                    (results are reported with DebugLog, which goes to stderr)
# Every program is also built with `blitzcc -o` and the executable must give the same
# output and exit status. blitzrun must sit next to blitzcc.
# $blitzpath is set to a scratch directory whose userlibs/ holds userlib/*.decls and the
# test dylib, so userlib programs run with blitzcc and as executables.
# Usage: run.sh /path/to/blitzcc /path/to/libblitztestlib.dylib [--update]
BLITZCC="$1"
TESTLIB="$2"
UPDATE="$3"
DIR="$(cd "$(dirname "$0")" && pwd)"
pass=0; fail=0
TMP="$(mktemp -d)"
mkdir -p "$TMP/home/userlibs"
cp "$DIR"/userlib/*.decls "$TMP/home/userlibs/"
cp "$TESTLIB" "$TMP/home/userlibs/"
export blitzpath="$TMP/home"
trap 'rm -rf "$TMP"' EXIT
for src in "$DIR"/*.bb; do
	name="$(basename "$src" .bb)"
	flags="$(sed -n 's/^; flags: *//p' "$src" | head -1)"
	if grep -q '^; graphics' "$src"; then
		# graphics program: real Graphics/Print on a hidden window with no GL (headless)
		export BB_HIDDEN=1 SDL_VIDEODRIVER=dummy
	else
		# everything else runs in text mode: Print writes to the terminal
		flags="-t $flags"
		unset BB_HIDDEN SDL_VIDEODRIVER
	fi
	want_rc="$(sed -n 's/^; rc: *//p' "$src" | head -1)"
	[ -z "$want_rc" ] && want_rc=0
	out="$("$BLITZCC" +q $flags "$src" 2>&1)"; rc=$?
	if [ "$UPDATE" = "--update" ]; then printf '%s\n' "$out" > "$DIR/$name.expected"; fi
	if [ "$out" = "$(cat "$DIR/$name.expected" 2>/dev/null)" ] && [ "$rc" = "$want_rc" ]; then
		pass=$((pass+1))
	else
		fail=$((fail+1)); echo "FAIL $name (rc=$rc, want $want_rc)"
		printf '%s\n' "$out" | diff - "$DIR/$name.expected" | head -10
	fi
	# a compile error (exit status 255) has no executable to build
	[ "$want_rc" = 255 ] && continue
	# the same program as a stand-alone executable (blitzcc -o) must behave identically
	exe="$TMP/$name"
	if ! "$BLITZCC" +q $flags -o "$exe" "$src" >/dev/null 2>&1; then
		fail=$((fail+1)); echo "FAIL $name: blitzcc -o failed"
		continue
	fi
	eout="$("$exe" 2>&1)"; erc=$?
	if [ "$eout" = "$(cat "$DIR/$name.expected")" ] && [ "$erc" = "$want_rc" ]; then
		pass=$((pass+1))
	else
		fail=$((fail+1)); echo "FAIL $name [exe] (rc=$erc, want $want_rc)"
		printf '%s\n' "$eout" | diff - "$DIR/$name.expected" | head -10
	fi
done
# malformed executables must be refused with exit status 2, not run
RUNNER="$(dirname "$BLITZCC")/blitzrun"
"$BLITZCC" +q -o "$TMP/good" "$DIR/hello.bb" >/dev/null 2>&1
gsize=$(wc -c < "$TMP/good" | tr -d ' ')
rsize=$(wc -c < "$RUNNER" | tr -d ' ')
cp "$RUNNER" "$TMP/nopayload"
head -c $((gsize-30)) "$TMP/good" > "$TMP/truncated"
cp "$TMP/good" "$TMP/corrupt"
printf 'X' | dd of="$TMP/corrupt" bs=1 seek=$((rsize+12)) conv=notrunc 2>/dev/null
for bad in nopayload truncated corrupt; do
	chmod +x "$TMP/$bad"
	"$TMP/$bad" >/dev/null 2>&1; brc=$?
	if [ "$brc" = 2 ]; then pass=$((pass+1)); else fail=$((fail+1)); echo "FAIL malformed executable '$bad' (rc=$brc, want 2)"; fi
done
echo "blitzcc: $pass passed, $fail failed"
[ "$fail" = 0 ]
