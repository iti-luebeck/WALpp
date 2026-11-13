"""Adapter package: upstream WAL's test-facing API, backed by the real WAL++ engine.

Pre-imported by run_conformance.py so that upstream's `from wal.core import Wal`
resolves here instead of to the Python re-implementation in tests/original/wal/.
"""
