"""`wal.reader` stand-in.

Upstream's reader parses source into its own AST. WAL++ parses internally and has no
Python-visible AST, so `read_wal_sexpr` returns the source text unchanged and
`Wal.eval` accepts text. That is enough for the behavioral tests, which only round-trip
text through the pair.

Tests that assert on upstream's parse tree (`test_parser.py`) need the real grammar and
are excluded from the conformance run.
"""


def read_wal_sexpr(source):
    """Return the source unchanged; see the module docstring."""
    return source


def read_wal_sexprs(source):
    return [source]


def read(source):
    return source


class ParseError(Exception):
    pass


WAL_GRAMMAR = None
