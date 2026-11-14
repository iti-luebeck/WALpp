#!/usr/bin/env bash
# verify.sh - check that this skill still describes the `wal` you actually have.
#
# Every claim in SKILL.md that can be checked by running something is checked
# here. Run it after pulling, after rebuilding, or whenever a documented command
# behaves unexpectedly.
#
# Why this exists: the earlier version of this skill documented four behaviors
# that were wrong. Two were never true (they were written from REPL output and
# assumed `-e` matched), and two described bugs that were later fixed. Nothing
# re-checked them, so every session inherited them. A silent wrong answer
# becomes a documented rule unless something falsifiable stands in the way.
#
# Usage:  ./verify.sh [path-to-wal]
# Exit:   0 all checks passed, 1 at least one failed.

set -uo pipefail

WAL_BIN="${1:-${WAL:-}}"
if [ -z "$WAL_BIN" ]; then
    WAL_BIN="$(command -v wal 2>/dev/null || true)"
fi
if [ -z "$WAL_BIN" ]; then
    # verify.sh -> skill dir -> skills -> .claude -> repo root
    guess="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)/build/wal"
    [ -x "$guess" ] && WAL_BIN="$guess"
fi
if [ ! -x "$WAL_BIN" ]; then
    echo "cannot find the wal binary. Pass it as an argument, set \$WAL, or build it."
    exit 1
fi

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"
TRACE="$REPO/tests/original/traces/counter.vcd"

pass=0
fail=0
skip=0

# check <description> <expected-substring> <actual>
check() {
    local what="$1" want="$2" got="$3"
    if [[ "$got" == *"$want"* ]]; then
        printf '  ok    %s\n' "$what"
        pass=$((pass + 1))
    else
        printf '  FAIL  %s\n       expected to contain: %q\n       got:                 %q\n' \
            "$what" "$want" "$got"
        fail=$((fail + 1))
    fi
}

note_skip() {
    printf '  skip  %s (%s)\n' "$1" "$2"
    skip=$((skip + 1))
}

echo "Verifying SKILL.md against: $WAL_BIN"
"$WAL_BIN" --version 2>&1 | head -1 | sed 's/^/  /'
echo

echo "Language and CLI semantics"
check "scalars echo from -e" \
    "3" "$("$WAL_BIN" -e '(+ 1 2)' 2>&1)"
check "no bare special variables (SIGNALS errors)" \
    "Undefined variable" "$("$WAL_BIN" -e 'SIGNALS' 2>&1)"
check "(define (f x) ...) is rejected" \
    "define target must be a symbol" "$("$WAL_BIN" -e '(define (f x) x)' 2>&1)"
check "let is a special form" \
    "3" "$("$WAL_BIN" -e '(let ([a 1] [b 2]) (+ a b))' 2>&1)"
check "upstream control flow is present" \
    "6" "$("$WAL_BIN" -e '(begin (define n 0) (while (< n 6) (set! n (+ n 1))) n)' 2>&1)"
check "defun and fn both define a function" \
    "42" "$("$WAL_BIN" -e '(begin (defun dbl [x] (* x 2)) (dbl 21))' 2>&1)"
check "the standard library is loaded" \
    "'rising" "$("$WAL_BIN" -e '(quote rising)' 2>&1)"
check "thresholded ops need their 2nd argument" \
    "expects 2 arguments" "$("$WAL_BIN" -e '(histogram (list 1 2 3))' 2>&1)"
check "indexing is 0-based" \
    "20" "$("$WAL_BIN" -e '(nth 1 (list 10 20 30))' 2>&1)"
check "analysis operators on explicit lists" \
    "2.73861" "$("$WAL_BIN" -e '(rms (list 1 2 3 4))' 2>&1)"

echo
echo "The two -e defects (fixed; older binaries will FAIL these)"
check "-e prints list results" \
    "(1 2 3)" "$("$WAL_BIN" -e '(list 1 2 3)' 2>&1)"
check "every -e runs, in order" \
    "one" "$("$WAL_BIN" -e '(print "one")' -e '(print "two")' 2>&1)"
check "-e flags share one interpreter" \
    "7" "$("$WAL_BIN" -e '(define x 5)' -e '(+ x 2)' 2>&1)"
check "top-level strings print bare" \
    "ab" "$("$WAL_BIN" -e '(concat "a" "b")' 2>&1)"

echo
echo "Trace handling"
if [ ! -f "$TRACE" ]; then
    note_skip "trace checks" "counter.vcd not found at $TRACE"
else
    check "signal names are dotted everywhere" \
        "tb.dut.counter" "$("$WAL_BIN" -l "$TRACE" -e '(print SIGNALS)' 2>&1)"
    check "the interpreter accepts a dotted name" \
        "true" "$("$WAL_BIN" -l "$TRACE" -e '(signal? "tb.dut.counter")' 2>&1)"
    check "the old ^ spelling still resolves" \
        "true" "$("$WAL_BIN" -l "$TRACE" -e '(signal? "tb^dut^counter")' 2>&1)"
    check "^ selects a trace id" \
        "80" "$("$WAL_BIN" -e '(load "'"$TRACE"'" "a")' -e 'a^MAX-INDEX' 2>&1)"
    check "upstream idioms run unchanged" \
        "40" "$("$WAL_BIN" -l "$TRACE" -e '(count (rising tb.clk))' 2>&1)"
    check "set-index moves the cursor (was a no-op stub)" \
        "40" "$("$WAL_BIN" -l "$TRACE" -e '(print (begin (set-index 40) (get-index)))' 2>&1)"
    check "navigation works inside one expression" \
        "15" "$("$WAL_BIN" -l "$TRACE" -e '(print (begin (set-index 40) (get-signal "tb^dut^counter")))' 2>&1)"
    check "map over range yields a real series" \
        "(0 0 0 0 0 0 0 0 0 0 0 1)" \
        "$("$WAL_BIN" -l "$TRACE" -e '(print (map (lambda (i) (begin (set-index i) (get-signal "tb^dut^counter"))) (range 0 12)))' 2>&1)"
    check "out-of-range index errors instead of clamping" \
        "out of range" "$("$WAL_BIN" -l "$TRACE" -e '(print (set-index 9999))' 2>&1)"

    # The cross-check that matters: interpreter and fast path must agree.
    interp="$("$WAL_BIN" -l "$TRACE" -e '(print (sum (map (lambda (p) (nth 1 p)) (hamming-distance-scope "tb"))))' 2>&1 | head -1)"
    tmp="$(mktemp -t walverify.XXXXXX)"
    "$WAL_BIN" --fast-hd-all "$TRACE" -o "$tmp" >/dev/null 2>&1
    fastsum="$(awk -F, '{s+=$2} END {print s}' "$tmp" 2>/dev/null)"
    rm -f "$tmp"
    check "interpreter total toggle count is 149" "149" "$interp"
    check "fast path agrees with the interpreter" "$interp" "$fastsum"

    # --fast-list speaks dotted names, unlike the interpreter.
    tmp2="$(mktemp -t walverify.XXXXXX)"
    "$WAL_BIN" --fast-list "$TRACE" -o "$tmp2" >/dev/null 2>&1
    check "--fast-list prints dotted names" "tb.dut.counter" "$(cat "$tmp2" 2>/dev/null)"
    check "--fast-list reports the timescale" "# timescale" "$(cat "$tmp2" 2>/dev/null)"
    rm -f "$tmp2"
fi

echo
echo "Agent modes"
check "--inspect reports the signal count" \
    '"signal_count":7' "$("$WAL_BIN" --compact-json --inspect "$TRACE" 2>&1)"
check "--inspect states the separator rather than assuming it" \
    '"hierarchy_separator":"."' "$("$WAL_BIN" --compact-json --inspect "$TRACE" 2>&1)"
check "--inspect gives names exactly as accepted" \
    '"name":"tb.dut.counter"' "$("$WAL_BIN" --compact-json --inspect "$TRACE" 2>&1)"
check "-e --json emits JSON in the envelope" \
    '"results":[[1,"two",true]]' "$("$WAL_BIN" --compact-json -e '(list 1 "two" #t)' 2>&1)"
check "a failure carries the same envelope" \
    '"ok":false' "$("$WAL_BIN" --compact-json -e '(bogus)' 2>/dev/null)"
check "--lint accepts a clean script" \
    "no problems found" "$(printf '(define a 1)\n(+ a 1)\n' > /tmp/wal_skill_ok.wal; "$WAL_BIN" --lint /tmp/wal_skill_ok.wal 2>&1)"
check "--lint catches a mistyped call" \
    "unknown-operator" "$(printf '(cont 1)\n' > /tmp/wal_skill_bad.wal; "$WAL_BIN" --lint /tmp/wal_skill_bad.wal 2>&1)"
check "the verb form equals the flag form" \
    "$("$WAL_BIN" --fast-hw-all "$TRACE" -o /tmp/wal_skill_f.csv >/dev/null 2>&1; md5 -q /tmp/wal_skill_f.csv 2>/dev/null || md5sum /tmp/wal_skill_f.csv | cut -d' ' -f1)" \
    "$("$WAL_BIN" extract weight "$TRACE" -o /tmp/wal_skill_v.csv >/dev/null 2>&1; md5 -q /tmp/wal_skill_v.csv 2>/dev/null || md5sum /tmp/wal_skill_v.csv | cut -d' ' -f1)"
rm -f /tmp/wal_skill_f.csv /tmp/wal_skill_v.csv
check "--inspect materialises the trace's age" \
    '"age_seconds"' "$("$WAL_BIN" --compact-json --inspect "$TRACE" 2>&1)"
rm -f /tmp/wal_skill_ok.wal /tmp/wal_skill_bad.wal

echo
echo "Bundled helper"
if [ -x "$HERE/scripts/wal-time.py" ]; then
    if [ -f "$TRACE" ]; then
        check "wal-time.py lists signals" \
            "tb.dut.counter" "$(WAL="$WAL_BIN" "$HERE/scripts/wal-time.py" "$TRACE" signals 2>&1)"
        check "wal-time.py reports ambiguity instead of guessing" \
            "ambiguous" "$(WAL="$WAL_BIN" "$HERE/scripts/wal-time.py" "$TRACE" edges clk 2>&1)"
    else
        note_skip "wal-time.py checks" "trace not found"
    fi
else
    note_skip "wal-time.py checks" "helper not executable"
fi

echo
echo "-----------------------------------------------"
printf 'passed %d, failed %d, skipped %d\n' "$pass" "$fail" "$skip"
if [ "$fail" -gt 0 ]; then
    cat <<'EOF'

Something in SKILL.md no longer matches this binary.

If the failures are in "The two -e defects", your `wal` predates the fix that
gave `-e` a list branch and made several -e accumulate. Either rebuild from a
revision that has it, or treat SKILL.md's "older binaries" note as current:
wrap list results in (print ...) and use one -e with begin.

Otherwise: trust the binary, not this file. Re-check the behavior by hand, fix
SKILL.md, and add the check here so the next person finds out cheaply.
EOF
    exit 1
fi
echo "SKILL.md matches this binary."
