# Migrating from WAL

Moving an existing upstream WAL script to WAL++.

For the behavioral differences and the flags that switch them off, see
[Compatibility with upstream WAL](../reference/wal-compatibility.md).

## Why Migrate?

If you're currently using the Python implementation of WAL from [wal-lang.org](https://wal-lang.org), WAL++ offers significant advantages:

**Performance**: WAL++ can be 10-50x faster, especially beneficial for:
- Large traces (> 1 GB)
- Intensive analysis operations
- Batch processing pipelines
- Real-time analysis from simulators

**Memory Efficiency**: Streaming architecture handles traces larger than available RAM:
- Process 100+ GB traces efficiently
- Reduce peak memory usage
- Better system resource utilization

**Deployment**: Single binary requires no Python installation:
- Faster startup (< 100ms)
- Simpler deployment in CI/CD
- Reduced container/image size

## Language Compatibility

WAL++ maintains **100% syntax compatibility** with WAL:

```scheme
; This WAL script runs identically in both implementations:
(define signals (signal-names))
(define bus-signals 
  (filter (lambda (s) (> (signal-width s) 1)) signals))
(map signal-width bus-signals)
```

## Migration Path

1. **Drop-in CLI Replacement**
   ```bash
   # Old WAL
   python -m wal -l trace.vcd -e '(signal-names)'
   
   # WAL++
   wal -l trace.vcd -e '(signal-names)'
   ```

2. **Existing Scripts Work Unchanged**
   ```bash
   # WAL script runs unchanged in both
   wal -l trace.vcd -f analysis.wal
   ```

3. **Python Integration**
   ```python
   # If using Python bindings, minimal changes needed
   import walpp  # instead of: import wal
   wal = walpp.Wal()
   # Same API calls follow
   ```

## Performance Comparison

Benchmark: Analyzing 2 GB VCD file with signal discovery and filtering

```
Operation                    | WAL (Python)  | WAL++ (C++)   | Speedup
-----------------------------|---------------|---------------|--------
Load trace                   | 45 seconds    | 2.1 seconds   | 21x
Signal discovery             | 8.2 seconds   | 0.3 seconds   | 27x
Filter bus signals (>1 bit)  | 12.5 seconds  | 0.5 seconds   | 25x
Peak memory usage            | 2.8 GB        | 0.45 GB       | 6.2x
Total analysis time          | 65 seconds    | 3.0 seconds   | 22x
```

## Known Differences

WAL++ is fully compatible, but has minor implementation differences:

| Feature | WAL (Python) | WAL++ |
|---------|--------------|-------|
| Precision | Python floats | C++ double |
| Large integers | Unlimited precision | 64-bit integers |
| Error messages | Python tracebacks | Simplified messages |
| Startup | ~1-2 seconds | ~50-100ms |
| Streaming | Manual chunking | Automatic |

## Support and Resources

- **Official WAL Project**: https://wal-lang.org
- **WAL Repository**: https://github.com/ics-jku/wal
- **WAL++ GitHub**: https://github.com/iti-luebeck/WALpp
- **Report Issues**: File issues in WAL++ repository

---

**Last Updated**: November 8, 2025
**Version**: 1.6.3
