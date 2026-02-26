import os
import sys
from pathlib import Path


def _find_wal_binary() -> Path:
    pkg_dir = Path(__file__).resolve().parent
    # CLI installed under walpp/bin via CMake when building wheel
    candidate = pkg_dir / "bin" / ("wal.exe" if os.name == "nt" else "wal")
    return candidate


def main() -> None:
    wal_bin = _find_wal_binary()
    if not wal_bin.exists():
        sys.stderr.write(
            "wal CLI binary not found in Python package.\n"
            "If you installed from source, run: cmake --build build && cmake --install build\n"
            "Or install a platform wheel that includes the CLI.\n"
        )
        sys.exit(1)

    # Replace current process with wal binary, forwarding all args
    os.execv(str(wal_bin), [str(wal_bin)] + sys.argv[1:])


if __name__ == "__main__":
    main()
