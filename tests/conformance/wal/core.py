"""`wal.core.Wal` backed by the real WAL++ engine.

Upstream WAL's test suite is written against `wal.core.Wal`. This adapter makes those
tests run against WAL++'s own pybind11 extension (`wal_cpp`), so a pass means WAL++
agrees with upstream, and a failure is a concrete parity gap.

This replaces what `tests/original/wal/` does. That package calls itself a shim but
contains a ~690-line Python interpreter of its own (`_eval_define`, `_eval_let`,
`_eval_lambda`, ...). It loads `libwalpp` with ctypes, warns if that fails, and then
evaluates in Python regardless, so the "original tests" never exercised WAL++ at all.
They were also not registered with CTest, so they never ran.

Values are converted to Python natives, because upstream's tests compare against plain
ints, floats, strings and lists.
"""
import os
import sys
from pathlib import Path

from .ast_defs import Symbol, WList, WalEvalError


def _load_extension():
    """Import the WAL++ extension module, searching the usual build locations."""
    try:
        import wal_cpp  # already importable
        return wal_cpp
    except ImportError:
        pass

    root = Path(__file__).resolve().parents[3]  # wal/ -> conformance/ -> tests/ -> repo
    candidates = [
        root / "build" / "lib",
        root / "build",
    ]
    env = os.environ.get("WALPP_BUILD_DIR")
    if env:
        candidates.insert(0, Path(env))

    for directory in candidates:
        if directory.is_dir() and any(directory.glob("wal_cpp*")):
            sys.path.insert(0, str(directory))
            import wal_cpp
            return wal_cpp

    raise ImportError(
        "cannot find the WAL++ extension module wal_cpp. Build it with\n"
        "  cmake --build build --target wal_cpp\n"
        "or set WALPP_BUILD_DIR to the directory containing it."
    )


_ext = _load_extension()


def to_python(value):
    """Convert a WAL++ Value into the plain Python value upstream's tests expect."""
    if not isinstance(value, _ext.Value):
        return value
    if value.is_nil():
        return None
    if value.is_bool():
        return value.to_bool()
    if value.is_int():
        return value.to_int()
    if value.is_float():
        return value.to_float()
    if value.is_string():
        return value.to_string()
    if value.is_symbol():
        return Symbol(value.to_symbol().get_name())
    if value.is_list():
        return WList(to_python(item) for item in value.to_list())
    return value


def _to_wal_literal(value):
    """Render a Python value as WAL source, for seeding variables."""
    if isinstance(value, bool):
        return "#t" if value else "#f"
    if isinstance(value, (int, float)):
        return repr(value)
    if isinstance(value, str):
        escaped = value.replace("\\", "\\\\").replace('"', '\\"')
        return f'"{escaped}"'
    if isinstance(value, Symbol):
        return f"'{value.name}"
    if isinstance(value, (list, tuple)):
        return "(list " + " ".join(_to_wal_literal(v) for v in value) + ")"
    if value is None:
        return "#nil"
    raise WalEvalError(f"cannot express {value!r} as a WAL literal")


class _Environment:
    """Upstream exposes `wal.eval_context.environment` for seeding test variables.

    WAL++'s bindings expose no environment object, so define/read are routed through
    the real evaluator. That keeps the engine authoritative rather than maintaining a
    second copy of the bindings here.
    """

    def __init__(self, wal):
        self._wal = wal

    def define(self, name, value):
        self._wal.eval_str(f"(define {name} {_to_wal_literal(value)})")

    def read(self, name):
        return self._wal.eval_str(name)

    def write(self, name, value):
        self._wal.eval_str(f"(set! {name} {_to_wal_literal(value)})")


class _EvalContext:
    """Stands in for upstream's eval context; only `environment` is used by the tests."""

    def __init__(self, wal):
        self.environment = _Environment(wal)
        self.global_environment = self.environment


class Wal:
    """Upstream's entry point, implemented over WAL++."""

    def __init__(self):
        self._wal = _ext.Wal()
        self.eval_context = _EvalContext(self)

    def load(self, file, tid="DEFAULT", **kwargs):
        try:
            return self._wal.load(file, tid)
        except Exception as exc:  # noqa: BLE001 - upstream tests expect WalEvalError
            raise WalEvalError(str(exc)) from exc

    def eval_str(self, txt, **args):
        if args:
            raise WalEvalError(
                "eval_str keyword arguments are not supported by the WAL++ adapter"
            )
        try:
            return to_python(self._wal.eval_str(txt))
        except Exception as exc:  # noqa: BLE001
            raise WalEvalError(str(exc)) from exc

    def eval(self, sexpr, **args):
        """Upstream evaluates a parsed AST; WAL++ parses from text.

        The conformance subset only uses `eval` on expressions obtained from
        `read_wal_sexpr`, which this adapter keeps as source text, so `eval` accepts
        that text back.
        """
        if isinstance(sexpr, str):
            return self.eval_str(sexpr, **args)
        raise WalEvalError(
            "the WAL++ adapter evaluates source text, not upstream AST objects"
        )
