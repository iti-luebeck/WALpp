# Installation

Building WAL++ from source, and the Python module.

## Prerequisites

- **C++20 capable compiler** (GCC 10+, Clang 11+, or MSVC 2019+)
- **CMake** 3.20+
- **Python** 3.8+ (for Python bindings)
- **pybind11** (for Python bindings)
- **libedit** (optional, line editing in the interactive REPL; `libedit-dev` on Debian/Ubuntu, built into macOS)
- **Optional**: pylibfst (for FST format support)

## Build from Source

```bash
# Clone the repository
git clone https://github.com/iti-luebeck/WALpp.git
cd WALpp

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_PYTHON_MODULE=ON \
         -DENABLE_TESTS=ON

# Build
cmake --build . -j$(nproc)

# Install
cmake --install .

# Run tests
ctest --output-on-failure
```

## Build Options

```bash
# Full build with all features
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_PYTHON_MODULE=ON \
         -DENABLE_TESTS=ON \
         -DENABLE_COVERAGE=ON \
         -DENABLE_FST_SUPPORT=ON

# Minimal build (C++ CLI only)
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DBUILD_PYTHON_MODULE=OFF \
         -DENABLE_TESTS=OFF

# Development build with debugging
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DBUILD_PYTHON_MODULE=ON \
         -DENABLE_TESTS=ON
```

## Python Installation

After building, install the Python module:

```bash
cd build
pip install -e .

# Or directly
python setup.py install
```

Verify installation:

```python
import walpp
print(walpp.__version__)
```

---
