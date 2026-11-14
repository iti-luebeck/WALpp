# Python API

Driving WAL++ from Python through the `wal_cpp` extension module.

## Core Classes

### `walpp.Wal`

Main WAL instance for evaluation and trace management.

```python
import walpp

# Create instance
wal = walpp.Wal()

# Evaluate expressions
result = wal.eval_str("(+ 1 2)")

# Load traces
wal.load("trace.vcd")

# Access environment
env = wal.get_environment()

# Get trace container
traces = wal.get_traces()
```

**Methods:**

| Method | Description |
|--------|-------------|
| `eval_str(expr)` | Evaluate WAL expression |
| `load(file, trace_id)` | Load waveform file |
| `unload(trace_id)` | Unload trace |
| `get_environment()` | Get variable environment |
| `get_operator_registry()` | Get operator registry |
| `get_traces()` | Get trace container |

### `walpp.Value`

Represents WAL expression values.

```python
# Check type
if result.is_int():
    print(result.to_int())
elif result.is_string():
    print(result.to_string())
elif result.is_list():
    print(result.to_list())
```

**Methods:**

| Method | Returns | Description |
|--------|---------|-------------|
| `is_int()` | bool | Check if integer |
| `is_float()` | bool | Check if float |
| `is_string()` | bool | Check if string |
| `is_list()` | bool | Check if list |
| `is_symbol()` | bool | Check if symbol |
| `is_bool()` | bool | Check if boolean |
| `is_nil()` | bool | Check if nil |
| `to_int()` | int | Convert to integer |
| `to_float()` | float | Convert to float |
| `to_string()` | str | Convert to string |
| `to_list()` | list | Convert to list |

## Example Scripts

### Example 1: Basic Trace Analysis

```python
#!/usr/bin/env python3
import walpp

# Create WAL instance
wal = walpp.Wal()

# Load trace
wal.load("simulation.vcd")

# Get signals
signals = wal.eval_str("(signal-names)")
print(f"Signals found: {signals}")

# Count signals
count = wal.eval_str("(length (signal-names))")
print(f"Total: {count.to_int()}")

# Signal widths
widths = wal.eval_str("(map signal-width (signal-names))")
print(f"Widths: {widths.to_list()}")
```

### Example 2: Filter and Analyze

```python
#!/usr/bin/env python3
import walpp

wal = walpp.Wal()
wal.load("trace.vcd")

# Define analysis in Python
wal.eval_str("""
(define all-signals (signal-names))
(define bus-signals 
  (filter (lambda (s) (> (signal-width s) 1)) all-signals))
(define control-signals
  (filter (lambda (s) (= (signal-width s) 1)) all-signals))
""")

# Retrieve results
buses = wal.eval_str("(length bus-signals)")
controls = wal.eval_str("(length control-signals)")

print(f"Bus signals: {buses.to_int()}")
print(f"Control signals: {controls.to_int()}")
```

### Example 3: Conditional Analysis

```python
#!/usr/bin/env python3
import walpp

wal = walpp.Wal()
wal.load("design.vcd")

signals = wal.eval_str("(signal-names)")
num_signals = len(signals) if hasattr(signals, '__len__') else 0

# Run different analysis based on design size
if num_signals > 100:
    result = wal.eval_str("""
    (list "Complex design"
          (length (signal-names)))
    """)
else:
    result = wal.eval_str("""
    (list "Simple design"
          (length (signal-names)))
    """)

print(result)
```

### Example 4: Jupyter Notebook Integration

```python
import walpp
import pandas as pd
import matplotlib.pyplot as plt

# Load and analyze trace
wal = walpp.Wal()
wal.load("simulation.vcd")

# Get signal properties
signals = wal.eval_str("(signal-names)")
widths = wal.eval_str("(map signal-width (signal-names))")

# Create DataFrame
df = pd.DataFrame({
    'Signal': signals,
    'Width': widths
})

# Visualization
df.groupby('Width').size().plot(kind='bar')
plt.title('Signal Distribution by Width')
plt.show()
```

---
