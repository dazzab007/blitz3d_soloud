#!/bin/sh
# Run every tests/blitzcc/*.bb through blitzcc and compare stdout+stderr with
# the .expected file next to it. Optional header comments in the .bb:
#   ; flags: -d      extra blitzcc flags
#   ; rc: 1          expected exit status (default 0)
# Usage: run.sh /path/to/blitzcc [--update]
BLITZCC="$1"
UPDATE="$2"
DIR="$(cd "$(dirname "$0")" && pwd)"
pass=0; fail=0
for src in "$DIR"/*.bb; do
	name="$(basename "$src" .bb)"
	flags="$(sed -n 's/^; flags: *//p' "$src" | head -1)"
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
done
echo "blitzcc: $pass passed, $fail failed"
[ "$fail" = 0 ]
