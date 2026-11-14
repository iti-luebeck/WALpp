# Migrating from WAL

Moving an existing upstream WAL script to WAL++.

For the behavioral differences and the flags that switch them off, see
[Compatibility with upstream WAL](../reference/wal-compatibility.md).

## Why Migrate?

If you're currently using the Python implementation of WAL from [wal-lang.org](https://wal-lang.org), WAL++ offers significant advantages:

**Performance**: WAL++ is substantially faster, most visibly for:
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

## Language compatibility

WAL++ runs the WAL language and loads upstream's own `lib/std/std.wal` unchanged, so
the vocabulary is the same one. Compatibility is close to full rather than total:
a few defaults differ deliberately, and
[Compatibility with upstream WAL](../reference/wal-compatibility.md) lists every one
with the flag that switches it back.

This script runs on both and prints the same number:

```scheme
(alias clk "cpu.clk")
(define cnt 0)
(while (step 1)
  (when (&& clk (! clk@-1))
    (set! cnt (+ cnt 1))))
(printf "Total rising edges: %d\n" cnt)
```

Note what it does *not* use. `signal-names`, `max-val` and `lambda` are WAL++
spellings with no upstream counterpart, where upstream has `SIGNALS` and `fn`. A
script meant to run on both should stay off them.

## Migration path

1. **The script itself usually needs no change.** Both engines take the program as a
   positional argument:

   ```bash
   wal analysis.wal -l trace.vcd       # upstream and WAL++
   wal -l trace.vcd -f analysis.wal    # WAL++ also accepts -f
   ```

2. **One expression from the command line is spelled differently.** Upstream's flag
   is `-c`, WAL++'s is `-e`:

   ```bash
   wal -c '(print SIGNALS)' -l trace.vcd    # upstream
   wal -e '(print SIGNALS)' -l trace.vcd    # WAL++
   ```

   WAL++ does not accept `-c`, and reads it as a filename, so a copied upstream
   command line fails with "Failed to open file" rather than doing the wrong thing
   quietly.

3. **A trace loaded with `-l` is `t0`,** then `t1`, as upstream names them, so
   `t0^tb.clk` resolves the same way on both.

4. **Python integration.** The wheel installs a `walpp` package:

   ```python
   import walpp          # instead of: import wal
   wal = walpp.Wal()
   ```

   The compiled extension underneath is `wal_cpp`, which is the name the in-tree
   conformance runner imports. From an installed wheel, use `walpp`.

## Performance

WAL++ is faster than upstream by a wide margin on anything trace-sized, because the
evaluator is compiled and the readers stream. The honest way to state it is to
measure your own workload, so here is the shape of it on one query rather than a
table of round numbers:

```
(count (rising top.sig0)) on a 50 MB VCD, 200 signals, 80,000 indices
  WAL++ 1.6.4        0.6 s
  upstream 0.9.1     did not finish inside 15 minutes
```

The paper's own measurements, with the workloads and scripts behind them, are in
[`artifacts/`](https://github.com/iti-luebeck/WALpp/tree/main/artifacts). Treat
anything else as unmeasured until you have run it.

## Known differences

The ones that change what a script computes are on
[Compatibility with upstream WAL](../reference/wal-compatibility.md), with the flags
that switch them off. The rest are differences in how the two engines behave rather
than in what they answer:

| Feature | WAL (Python) | WAL++ |
|---------|--------------|-------|
| Large integers | Unlimited precision | 64-bit, `--bigint` for exact |
| Type coercion | Python's | strict, `--compat-types` for upstream's |
| The empty value prints as | `None` | `nil` |
| Error messages | Python tracebacks | one line |
| Startup | ~1-2 seconds | under 100 ms |
| Streaming | Manual chunking | Automatic above 100 MB |

`print` output itself agrees: booleans are `true` and `false`, a float carries the
shortest decimal that reads back as the same value and keeps its `.0` when whole,
arguments run together with no separator, a top-level string is bare while a nested
one is quoted, and arrays render as `{("x" 10)}`. So an upstream script's output can
be diffed against WAL++'s.

## Support and Resources

- **Official WAL Project**: https://wal-lang.org
- **WAL Repository**: https://github.com/ics-jku/wal
- **WAL++ GitHub**: https://github.com/iti-luebeck/WALpp
- **Report Issues**: File issues in WAL++ repository

---

**Last Updated**: November 14, 2026
**Version**: 1.6.4
