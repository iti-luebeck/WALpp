"""Minimal stand-ins for upstream WAL's AST types.

Upstream's test suite imports these from `wal.ast_defs`. Only what the behavioral
tests actually touch is provided: the error types, `Symbol`, `WList`, and an
`Operator` placeholder. The AST-shape tests (`test_parser.py`, `test_varresolve.py`)
assert on upstream's internal representation and are excluded from the conformance
run, so nothing here needs to reproduce that representation faithfully.
"""


class WalError(Exception):
    """Base error, as upstream."""


class WalEvalError(WalError):
    """Raised when evaluation fails. Upstream's tests assert on this type."""


class Symbol:
    """A WAL symbol. Compares by name so tests can assert on returned symbols."""

    def __init__(self, name, _depth=None):
        self.name = name

    def __eq__(self, other):
        if isinstance(other, Symbol):
            return self.name == other.name
        return NotImplemented

    def __hash__(self):
        return hash(self.name)

    def __lt__(self, other):
        # Upstream's tests sort lists of symbols before comparing them.
        if isinstance(other, Symbol):
            return self.name < other.name
        return NotImplemented

    def __repr__(self):
        return f"Symbol({self.name!r})"


class WList(list):
    """Upstream represents WAL lists as a list subclass; plain lists compare equal."""


class Operator:
    """Placeholder.

    Upstream's `Operator` is an enum of builtin names used by its AST-shape tests.
    Those tests are excluded from the conformance run, so attribute access returns a
    marker rather than pretending to mirror upstream's enum.
    """

    def __getattr__(self, item):  # pragma: no cover - only hit by excluded tests
        raise AttributeError(
            f"Operator.{item} is upstream-internal; AST-shape tests are excluded "
            "from the conformance run (see run_conformance.py)"
        )
