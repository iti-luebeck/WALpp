#!/usr/bin/env python3
"""
Python Bindings Test Suite for WAL++

Tests the pybind11 Python module for WAL++
"""

import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'build', 'lib'))

try:
    import wal_cpp as wal
    print("✓ Successfully imported wal_cpp module")
except ImportError as e:
    print(f"✗ Failed to import wal_cpp: {e}")
    sys.exit(1)

def test_basic_values():
    """Test basic Value creation and operations"""
    print("\n=== Testing Basic Values ===")
    
    # Create values
    val_int = wal.make_value_int(42)
    assert val_int.is_int(), "Should be int"
    assert val_int.to_int() == 42, "Should convert to 42"
    print("✓ Integer value creation and conversion")
    
    val_float = wal.make_value_float(3.14)
    assert val_float.is_float(), "Should be float"
    assert abs(val_float.to_float() - 3.14) < 0.01, "Should convert to 3.14"
    print("✓ Float value creation and conversion")
    
    val_str = wal.make_value_string("hello")
    assert val_str.is_string(), "Should be string"
    assert val_str.to_string() == "hello", "Should convert to 'hello'"
    print("✓ String value creation and conversion")
    
    val_sym = wal.make_value_symbol("x")
    assert val_sym.is_symbol(), "Should be symbol"
    assert val_sym.to_symbol().get_name() == "x", "Should have name 'x'"
    print("✓ Symbol value creation and conversion")

def test_list_operations():
    """Test list creation and manipulation"""
    print("\n=== Testing Lists ===")
    
    lst = wal.make_list()
    assert len(lst) == 0, "Should start empty"
    print("✓ Empty list creation")
    
    lst.append(wal.make_value_int(1))
    lst.append(wal.make_value_int(2))
    lst.append(wal.make_value_int(3))
    assert len(lst) == 3, "Should have 3 elements"
    print("✓ List append and size")
    
    first = lst.__getitem__(0)
    assert first.to_int() == 1, "First element should be 1"
    print("✓ List element access")

def test_wal_creation():
    """Test WAL instance creation"""
    print("\n=== Testing WAL Instance ===")
    
    w = wal.Wal()
    print("✓ WAL instance created")
    
    # Try a simple evaluation
    result = w.eval_str("(+ 1 2)")
    assert result.is_int(), "Result should be int"
    assert result.to_int() == 3, "1 + 2 should be 3"
    print("✓ Simple arithmetic: (+ 1 2) = 3")

def test_variable_binding():
    """Test variable definition and lookup"""
    print("\n=== Testing Variables ===")
    
    w = wal.Wal()
    
    # Define a variable
    w.define("x", wal.make_value_int(10))
    
    # Get it back
    val = w.get_var("x")
    assert val.to_int() == 10, "Should get back 10"
    print("✓ Variable definition and lookup")
    
    # Update it
    w.set_var("x", wal.make_value_int(20))
    val = w.get_var("x")
    assert val.to_int() == 20, "Should be updated to 20"
    print("✓ Variable update")

def test_custom_operator():
    """Test registering a custom operator from Python"""
    print("\n=== Testing Custom Operators ===")
    
    w = wal.Wal()
    
    # Define a simple Python function
    def my_double(eval_ctx, args):
        if len(args) != 1:
            raise ValueError("my-double expects 1 argument")
        # args are shared_ptr<Value>, convert to int, double, return
        val = args[0]
        if val.is_int():
            return wal.make_value_int(val.to_int() * 2)
        else:
            return wal.make_value_float(val.to_float() * 2)
    
    # Register the operator
    w.register_operator("my-double", my_double)
    print("✓ Custom operator registered")
    
    # Try to use it
    result = w.eval_str("(my-double 5)")
    assert result.to_int() == 10, "my-double 5 should be 10"
    print("✓ Custom operator works: (my-double 5) = 10")

def test_environment():
    """Test environment operations"""
    print("\n=== Testing Environment ===")
    
    env = wal.Environment()
    
    # Define a variable
    env.define("test", wal.make_value_int(42))
    assert env.is_defined("test"), "Variable should be defined"
    print("✓ Environment variable definition")
    
    val = env.get("test")
    assert val.to_int() == 42, "Should get back 42"
    print("✓ Environment variable lookup")
    
    # Write (update) a variable
    env.write("test", wal.make_value_int(100))
    val = env.get("test")
    assert val.to_int() == 100, "Should be updated to 100"
    print("✓ Environment variable update")

def test_exceptions():
    """Test exception handling"""
    print("\n=== Testing Exceptions ===")
    
    w = wal.Wal()
    
    try:
        w.eval_str("(undefined-var)")
        assert False, "Should have raised error"
    except (wal.EvalError, RuntimeError) as e:
        if "Undefined variable" in str(e):
            print("✓ Eval exception raised correctly")
        else:
            raise

def main():
    print("╔════════════════════════════════════════╗")
    print("║   WAL++ Python Bindings Test Suite    ║")
    print("╚════════════════════════════════════════╝")
    
    try:
        test_basic_values()
        test_list_operations()
        test_wal_creation()
        test_variable_binding()
        test_custom_operator()
        test_environment()
        test_exceptions()
        
        print("\n" + "="*50)
        print("✓ ALL TESTS PASSED!")
        print("="*50)
        return 0
    except AssertionError as e:
        print(f"\n✗ Test failed: {e}")
        return 1
    except Exception as e:
        print(f"\n✗ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())
