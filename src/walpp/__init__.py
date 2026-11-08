# walpp Python package
# This package exposes the compiled pybind11 extension built by CMake.

try:
    # The extension is installed as walpp/wal_cpp.*.so
    from . import wal_cpp as _ext
except Exception as e:
    raise ImportError("Failed to import walpp extension 'wal_cpp'\n" + str(e))

# Re-export public API from the extension
__all__ = [name for name in dir(_ext) if not name.startswith("_")]
for name in __all__:
    globals()[name] = getattr(_ext, name)

# Optional: expose version if provided by extension
try:
    from .wal_cpp import __version__  # type: ignore
except Exception:
    __version__ = "1.2.2"
