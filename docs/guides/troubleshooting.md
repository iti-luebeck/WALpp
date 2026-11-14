# Troubleshooting

What the common failures mean and what to do about them.

## Common Issues

### 1. "Unknown file format"

**Problem:** File format not recognized or extension incorrect

**Solution:**
```bash
# Check file format
file trace.vcd

# Force format detection by piping
cat trace.vcd | wal --stdin

# Rename file with correct extension
mv trace.txt trace.vcd
```

### 2. "Failed to load trace"

**Problem:** Trace file is corrupt or malformed

**Solution:**
```bash
# Validate VCD format
# Check first few lines
head -20 trace.vcd

# Try with no-streaming
wal -l trace.vcd --no-streaming

# Check file permissions
ls -l trace.vcd
chmod 644 trace.vcd
```

### 3. "Out of memory"

**Problem:** Large trace file consumes too much memory

**Solution:**
```bash
# Use streaming mode
wal -l huge.vcd --streaming

# Reduce chunk size
wal -l huge.vcd --streaming --chunk-size 50

# Reduce cache capacity
wal -l huge.vcd --cache-capacity 1000

# Process in chunks manually
cat huge.vcd | wal --stdin --chunk-size 64
```

### 4. "Undefined variable"

**Problem:** Variable or operator not found

**Solution:**
```bash
# Check variable is defined
wal -e '(define x 42)'
wal -e 'x'

# Verify signal names
wal -l trace.vcd -e '(signal-names)'

# Check for typos
# Did you mean (signal-names) not (signal-name)?
```

### 5. "Type mismatch error"

**Problem:** Operation applied to wrong type

**Solution:**
```bash
# Type check before operation
wal -e '(if (is-list x) (length x) 0)'

# Convert types explicitly
wal -e '(string-length (number->string 42))'

# Use correct operators for types
wal -e '(+ 1 2)'        ; Integer addition
wal -e '(concat "a" "b")' ; String concatenation
```

## Performance Tips

1. **Use streaming for large files**
   ```bash
   wal -l huge.vcd --streaming
   ```

2. **Predefine variables in scripts**
   ```scheme
   (define signals (signal-names))
   (define bus-signals (filter ...))
   ```

3. **Use relative operations**
   ```bash
   # Instead of full list processing each time
   wal -l trace.vcd -e '(define sigs (signal-names))' \
      -e '(length sigs)'
   ```

4. **Avoid unnecessary list conversions**
   ```scheme
   ; Good - single pass
   (map signal-width (signal-names))
   
   ; Less efficient - multiple passes
   (map signal-width (signal-names))
   (map signal-width (signal-names))
   ```

---

## FAQ

**Q: What's the difference between --pipe and --stdin?**

A: `--stdin` reads from standard input (pipes), while `--pipe` reads from a named pipe (FIFO) file. Use `--stdin` for shell pipes (`cat file | wal --stdin`) and `--pipe` for named pipes (`wal --pipe /tmp/trace.fifo`).

**Q: Can I combine multiple input modes?**

A: No, you can use one input mode at a time: `-l` (file), `--stdin`, or `--pipe`.

**Q: How do I process very large traces?**

A: Use streaming mode: `wal -l huge.vcd --streaming`. Adjust `--chunk-size` and `--cache-capacity` as needed.

**Q: Can I save processed waveforms?**

A: Yes, WAL++ includes write operators: `write-vcd`, `write-csv`, `write-fst`.

**Q: How do I integrate with my simulation flow?**

A: Pipe simulator output directly: `simulator --vcd /dev/stdout | wal --stdin -f analysis.wal`

**Q: Is Python module required?**

A: No, the CLI works standalone. Python module is optional for scripting integration.

---
