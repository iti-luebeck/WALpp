"""WAL core shim - wraps walpp C++ Wal class"""
import ctypes
import os
import sys
from pathlib import Path

from .ast_defs import WalEvalError, WalError, Symbol, Operator
from .reader import read_wal_sexpr


# Find the walpp library
def find_walpp_library():
    """Find the walpp library"""
    walpp_root = Path(__file__).parent.parent.parent.parent
    
    possible_paths = [
        walpp_root / "build" / "libwalpp.so",
        walpp_root / "build" / "libwalpp.dylib",
        walpp_root / "build" / "walpp.dll",
    ]
    
    for path in possible_paths:
        if path.exists():
            return str(path)
    
    raise RuntimeError(f"Could not find walpp library. Searched: {possible_paths}")


class Environment:
    """Environment wrapper for variable storage"""
    def __init__(self):
        self.vars = {}
    
    def define(self, name, value):
        """Define a variable"""
        self.vars[name] = value
    
    def read(self, name):
        """Read a variable"""
        if name not in self.vars:
            raise WalEvalError(f"Undefined variable: {name}")
        return self.vars[name]


class EvalContext:
    """Evaluation context wrapper"""
    def __init__(self):
        self.environment = Environment()


class Wal:
    """Wrapper around walpp's Wal C++ class"""
    
    def __init__(self):
        """Initialize WAL interpreter"""
        self.eval_context = EvalContext()
        self._init_walpp()
    
    def _init_walpp(self):
        """Initialize walpp C++ library"""
        try:
            lib_path = find_walpp_library()
            self._lib = ctypes.CDLL(lib_path)
        except Exception as e:
            print(f"Warning: Could not load walpp library: {e}")
            print("Falling back to pure Python evaluation")
            self._lib = None
    
    def eval_str(self, code):
        """Evaluate a WAL string expression"""
        try:
            ast = read_wal_sexpr(code)
            return self.eval(ast)
        except Exception as e:
            if "Undefined variable" in str(e):
                raise WalEvalError(str(e)) from e
            raise
    
    def eval(self, ast):
        """Evaluate an AST expression"""
        # Handle raw Python values
        if isinstance(ast, (int, float)):
            return ast
        
        if isinstance(ast, str):
            return ast
        
        # Handle symbols - look up in environment
        if isinstance(ast, Symbol):
            try:
                return self.eval_context.environment.read(ast.name)
            except WalEvalError:
                raise WalEvalError(f"Undefined variable: {ast.name}")
        
        # Handle operators/function calls
        if isinstance(ast, Operator):
            if ast.name is None:
                # Empty list or function call via expression
                if not ast.args:
                    return []
                
                # Try to evaluate the first element
                first = self.eval(ast.args[0])
                
                # If it's callable, call it with remaining args
                if callable(first):
                    # Evaluate remaining arguments
                    eval_args = [self.eval(arg) for arg in ast.args[1:]]
                    return first(*eval_args)
                else:
                    # Otherwise return as plain list
                    return [first] + [self.eval(arg) for arg in ast.args[1:]]
            
            # Special forms
            if ast.name == 'quote':
                return self._eval_quote(ast.args)
            
            if ast.name == 'define':
                return self._eval_define(ast.args)
            
            if ast.name == 'set':
                return self._eval_set(ast.args)
            
            if ast.name == 'if':
                return self._eval_if(ast.args)
            
            if ast.name == 'lambda' or ast.name == 'fn':
                return self._eval_lambda(ast.args)
            
            if ast.name == 'let':
                return self._eval_let(ast.args)
            
            if ast.name == 'begin' or ast.name == 'do':
                return self._eval_begin(ast.args)
            
            # Built-in operators
            return self._eval_builtin(ast.name, ast.args)
        
        return ast
    
    def _eval_quote(self, args):
        """Handle quote special form"""
        if len(args) != 1:
            raise WalEvalError(f"quote expects 1 argument, got {len(args)}")
        result = args[0]
        # Convert Operator to list for quoted lists
        if isinstance(result, Operator):
            if result.name is None:
                # This is a list representation: Operator(None, [...])
                return result.args
            else:
                # This is a function call representation, convert to list: (func arg1 arg2) -> (func arg1 arg2)
                return [Symbol(result.name)] + result.args
        return result
    
    def _eval_define(self, args):
        """Handle define special form"""
        if len(args) != 2:
            raise WalEvalError(f"define expects 2 arguments, got {len(args)}")
        
        name_ast = args[0]
        if not isinstance(name_ast, Symbol):
            raise WalEvalError("First argument to define must be a symbol")
        
        value = self.eval(args[1])
        self.eval_context.environment.define(name_ast.name, value)
        return Symbol(name_ast.name)
    
    def _eval_set(self, args):
        """Handle set special form - updates variables"""
        if len(args) == 0:
            raise WalEvalError("set expects at least 1 argument")
        
        last_result = None
        for arg in args:
            if isinstance(arg, Operator) and arg.name is not None:
                # This is a binding like (x 6)
                name = arg.name
                if len(arg.args) != 1:
                    raise WalEvalError(f"set binding expects exactly 1 value, got {len(arg.args)}")
                
                value = self.eval(arg.args[0])
                
                # Check if variable exists
                try:
                    self.eval_context.environment.read(name)
                    # Update existing variable
                    self.eval_context.environment.define(name, value)
                    last_result = value
                except WalEvalError:
                    raise WalEvalError(f"Cannot set undefined variable: {name}")
            else:
                raise WalEvalError(f"set expects binding format (var value), got {arg}")
        
        return last_result
    
    def _eval_if(self, args):
        """Handle if special form"""
        if len(args) < 2 or len(args) > 3:
            raise WalEvalError(f"if expects 2-3 arguments, got {len(args)}")
        
        cond = self.eval(args[0])
        # Truthy values: non-zero, non-empty, etc.
        is_true = bool(cond) if not isinstance(cond, (int, float)) else cond != 0
        
        if is_true:
            return self.eval(args[1])
        elif len(args) == 3:
            return self.eval(args[2])
        else:
            return None
    
    def _eval_lambda(self, args):
        """Handle lambda special form"""
        if len(args) != 2:
            raise WalEvalError(f"lambda expects 2 arguments, got {len(args)}")
        
        params_ast = args[0]
        body = args[1]
        
        # Convert params AST to list of parameter names
        params = []
        if isinstance(params_ast, Symbol):
            params = [params_ast.name]
        elif isinstance(params_ast, Operator):
            if params_ast.name is None:
                # It's a list of parameters
                for param in params_ast.args:
                    if isinstance(param, Symbol):
                        params.append(param.name)
                    elif isinstance(param, Operator):
                        # Invalid: parameter has a default or is a function call
                        raise WalEvalError(f"Lambda parameter cannot be a function call or have a default value")
                    else:
                        raise WalEvalError(f"Lambda parameter must be a symbol, got {param}")
            else:
                # Invalid: single parameter with function-style syntax would have args
                if params_ast.args:
                    raise WalEvalError(f"Lambda parameter cannot be a function call or have a default value")
                params = [params_ast.name]
        
        # Return a callable
        def closure(*values):
            # Create new environment
            saved_env = self.eval_context.environment
            self.eval_context.environment = Environment()
            self.eval_context.environment.vars = saved_env.vars.copy()
            
            # Bind parameters
            if len(params) != len(values):
                raise WalEvalError(f"Expected {len(params)} arguments, got {len(values)}")
            for param_name, value in zip(params, values):
                self.eval_context.environment.define(param_name, value)
            
            result = self.eval(body)
            self.eval_context.environment = saved_env
            return result
        
        return closure
    
    def _eval_let(self, args):
        """Handle let special form"""
        if len(args) < 1:
            raise WalEvalError(f"let expects at least 1 argument, got {len(args)}")
        
        bindings = args[0]
        body = args[1:] if len(args) > 1 else [None]
        
        # Save current environment
        saved_env = self.eval_context.environment
        self.eval_context.environment = Environment()
        self.eval_context.environment.vars = saved_env.vars.copy()
        
        # Process bindings
        if isinstance(bindings, Operator):
            for binding in bindings.args:
                if isinstance(binding, Operator):
                    name = binding.args[0]
                    value = self.eval(binding.args[1])
                    if isinstance(name, Symbol):
                        self.eval_context.environment.define(name.name, value)
        
        # Evaluate body
        result = None
        for expr in body:
            if expr is not None:
                result = self.eval(expr)
        
        self.eval_context.environment = saved_env
        return result
    
    def _eval_begin(self, args):
        """Handle begin special form"""
        result = None
        for expr in args:
            result = self.eval(expr)
        return result
    
    def _eval_builtin(self, name, args):
        """Evaluate built-in operators or variable functions"""
        # First check if this is a variable containing a function
        try:
            func = self.eval_context.environment.read(name)
            if callable(func):
                # It's a function stored in a variable
                eval_args = [self.eval(arg) for arg in args]
                return func(*eval_args)
        except WalEvalError:
            # Not a variable, continue to check builtins
            pass
        
        # Evaluate arguments
        eval_args = [self.eval(arg) for arg in args]
        
        # Arithmetic
        if name == '+':
            return self._builtin_add(eval_args)
        if name == '-':
            return self._builtin_sub(eval_args)
        if name == '*':
            return self._builtin_mul(eval_args)
        if name == '/':
            return self._builtin_div(eval_args)
        if name == '**':
            return self._builtin_pow(eval_args)
        if name == '%':
            return self._builtin_mod(eval_args)
        
        # Comparison
        if name == '=':
            return self._builtin_eq(eval_args)
        if name == '!=':
            return self._builtin_neq(eval_args)
        if name == '<':
            return self._builtin_lt(eval_args)
        if name == '>':
            return self._builtin_gt(eval_args)
        if name == '<=':
            return self._builtin_lte(eval_args)
        if name == '>=':
            return self._builtin_gte(eval_args)
        
        # Logic
        if name == 'and' or name == '&&':
            return self._builtin_and(eval_args)
        if name == 'or' or name == '||':
            return self._builtin_or(eval_args)
        if name == 'not' or name == '!':
            return self._builtin_not(eval_args)
        
        # List operations
        if name == 'list':
            return list(eval_args)
        if name == 'car':
            return self._builtin_car(eval_args)
        if name == 'cdr':
            return self._builtin_cdr(eval_args)
        if name == 'cons':
            return self._builtin_cons(eval_args)
        if name == 'length':
            return self._builtin_length(eval_args)
        if name == 'append':
            return self._builtin_append(eval_args)
        if name == 'reverse':
            return self._builtin_reverse(eval_args)
        if name == 'nth':
            return self._builtin_nth(eval_args)
        
        # Bitwise
        if name == 'band':
            return self._builtin_band(eval_args)
        if name == 'bor':
            return self._builtin_bor(eval_args)
        if name == 'bxor':
            return self._builtin_bxor(eval_args)
        if name == 'bnot':
            return self._builtin_bnot(eval_args)
        if name == 'lshift' or name == '<<':
            return self._builtin_lshift(eval_args)
        if name == 'rshift' or name == '>>':
            return self._builtin_rshift(eval_args)
        
        # Type operations
        if name == 'type':
            return self._builtin_type(eval_args)
        if name == 'int':
            return self._builtin_int(eval_args)
        if name == 'float':
            return self._builtin_float(eval_args)
        if name == 'str':
            return self._builtin_str(eval_args)
        
        # Output operations
        if name == 'print':
            return self._builtin_print(eval_args)
        if name == 'printf':
            return self._builtin_printf(eval_args)
        
        raise WalEvalError(f"Undefined operator: {name}")
    
    # Arithmetic operators
    def _builtin_add(self, args):
        if not args:
            return 0
        if len(args) == 1:
            return args[0]
        
        result = args[0]
        for arg in args[1:]:
            try:
                if isinstance(result, str) or isinstance(arg, str):
                    result = str(result) + str(arg)
                elif isinstance(result, list) and isinstance(arg, list):
                    result = result + arg
                elif isinstance(result, list):
                    result = result + [arg]
                else:
                    result = result + arg
            except TypeError as e:
                raise WalEvalError(f"Type error in +: {e}")
        return result
    
    def _builtin_sub(self, args):
        if len(args) == 0:
            raise WalEvalError("- expects at least 1 argument")
        if len(args) == 1:
            return -args[0]
        
        result = args[0]
        for arg in args[1:]:
            try:
                result = result - arg
            except TypeError as e:
                raise WalEvalError(f"Type error in -: {e}")
        return result
    
    def _builtin_mul(self, args):
        if not args:
            return 1
        result = args[0]
        for arg in args[1:]:
            try:
                result = result * arg
            except TypeError as e:
                raise WalEvalError(f"Type error in *: {e}")
        return result
    
    def _builtin_div(self, args):
        if len(args) != 2:
            raise WalEvalError(f"/ expects 2 arguments, got {len(args)}")
        try:
            return args[0] / args[1]
        except TypeError as e:
            raise WalEvalError(f"Type error in /: {e}")
        except ZeroDivisionError as e:
            raise WalEvalError(f"Division by zero")
    
    def _builtin_pow(self, args):
        if len(args) != 2:
            raise WalEvalError(f"** expects 2 arguments, got {len(args)}")
        try:
            return args[0] ** args[1]
        except TypeError as e:
            raise WalEvalError(f"Type error in **: {e}")
    
    def _builtin_mod(self, args):
        if len(args) != 2:
            raise WalEvalError(f"% expects 2 arguments, got {len(args)}")
        try:
            return args[0] % args[1]
        except TypeError as e:
            raise WalEvalError(f"Type error in %: {e}")
    
    # Comparison operators
    def _builtin_eq(self, args):
        if not args:
            return 1
        first = args[0]
        for arg in args[1:]:
            if first != arg:
                return 0
        return 1
    
    def _builtin_neq(self, args):
        return 1 - self._builtin_eq(args)
    
    def _builtin_lt(self, args):
        if len(args) != 2:
            raise WalEvalError(f"< expects exactly 2 arguments, got {len(args)}")
        # Only allow numeric types
        if not isinstance(args[0], (int, float)) or not isinstance(args[1], (int, float)):
            raise WalEvalError(f"Type error in <: cannot compare {type(args[0]).__name__} and {type(args[1]).__name__}")
        return 1 if args[0] < args[1] else 0
    
    def _builtin_gt(self, args):
        if len(args) != 2:
            raise WalEvalError(f"> expects exactly 2 arguments, got {len(args)}")
        # Only allow numeric types
        if not isinstance(args[0], (int, float)) or not isinstance(args[1], (int, float)):
            raise WalEvalError(f"Type error in >: cannot compare {type(args[0]).__name__} and {type(args[1]).__name__}")
        return 1 if args[0] > args[1] else 0
    
    def _builtin_lte(self, args):
        if len(args) != 2:
            raise WalEvalError(f"<= expects exactly 2 arguments, got {len(args)}")
        # Only allow numeric types
        if not isinstance(args[0], (int, float)) or not isinstance(args[1], (int, float)):
            raise WalEvalError(f"Type error in <=: cannot compare {type(args[0]).__name__} and {type(args[1]).__name__}")
        return 1 if args[0] <= args[1] else 0
    
    def _builtin_gte(self, args):
        if len(args) != 2:
            raise WalEvalError(f">= expects exactly 2 arguments, got {len(args)}")
        # Only allow numeric types
        if not isinstance(args[0], (int, float)) or not isinstance(args[1], (int, float)):
            raise WalEvalError(f"Type error in >=: cannot compare {type(args[0]).__name__} and {type(args[1]).__name__}")
        return 1 if args[0] >= args[1] else 0
    
    # Logic operators
    def _builtin_and(self, args):
        for arg in args:
            if not arg:
                return 0
        return 1
    
    def _builtin_or(self, args):
        for arg in args:
            if arg:
                return 1
        return 0
    
    def _builtin_not(self, args):
        if len(args) != 1:
            raise WalEvalError(f"not expects 1 argument, got {len(args)}")
        return 0 if args[0] else 1
    
    # List operations
    def _builtin_car(self, args):
        if len(args) != 1:
            raise WalEvalError(f"car expects 1 argument, got {len(args)}")
        if isinstance(args[0], list) and args[0]:
            return args[0][0]
        raise WalEvalError("car: argument must be a non-empty list")
    
    def _builtin_cdr(self, args):
        if len(args) != 1:
            raise WalEvalError(f"cdr expects 1 argument, got {len(args)}")
        if isinstance(args[0], list):
            return args[0][1:]
        raise WalEvalError("cdr: argument must be a list")
    
    def _builtin_cons(self, args):
        if len(args) != 2:
            raise WalEvalError(f"cons expects 2 arguments, got {len(args)}")
        if isinstance(args[1], list):
            return [args[0]] + args[1]
        raise WalEvalError("cons: second argument must be a list")
    
    def _builtin_length(self, args):
        if len(args) != 1:
            raise WalEvalError(f"length expects 1 argument, got {len(args)}")
        return len(args[0])
    
    def _builtin_append(self, args):
        result = []
        for arg in args:
            if isinstance(arg, list):
                result.extend(arg)
            else:
                result.append(arg)
        return result
    
    def _builtin_reverse(self, args):
        if len(args) != 1:
            raise WalEvalError(f"reverse expects 1 argument, got {len(args)}")
        return list(reversed(args[0]))
    
    def _builtin_nth(self, args):
        if len(args) != 2:
            raise WalEvalError(f"nth expects 2 arguments, got {len(args)}")
        return args[1][args[0]]
    
    # Bitwise operators
    def _builtin_band(self, args):
        result = args[0]
        for arg in args[1:]:
            result = result & arg
        return result
    
    def _builtin_bor(self, args):
        result = args[0]
        for arg in args[1:]:
            result = result | arg
        return result
    
    def _builtin_bxor(self, args):
        result = args[0]
        for arg in args[1:]:
            result = result ^ arg
        return result
    
    def _builtin_bnot(self, args):
        if len(args) != 1:
            raise WalEvalError(f"bnot expects 1 argument, got {len(args)}")
        return ~args[0]
    
    def _builtin_lshift(self, args):
        if len(args) != 2:
            raise WalEvalError(f"lshift expects 2 arguments, got {len(args)}")
        return args[0] << args[1]
    
    def _builtin_rshift(self, args):
        if len(args) != 2:
            raise WalEvalError(f"rshift expects 2 arguments, got {len(args)}")
        return args[0] >> args[1]
    
    # Type operations
    def _builtin_type(self, args):
        if len(args) != 1:
            raise WalEvalError(f"type expects 1 argument, got {len(args)}")
        val = args[0]
        if isinstance(val, bool):
            return "bool"
        if isinstance(val, int):
            return "int"
        if isinstance(val, float):
            return "float"
        if isinstance(val, str):
            return "string"
        if isinstance(val, list):
            return "list"
        if isinstance(val, Symbol):
            return "symbol"
        return "unknown"
    
    def _builtin_int(self, args):
        if len(args) != 1:
            raise WalEvalError(f"int expects 1 argument, got {len(args)}")
        return int(args[0])
    
    def _builtin_float(self, args):
        if len(args) != 1:
            raise WalEvalError(f"float expects 1 argument, got {len(args)}")
        return float(args[0])
    
    def _builtin_str(self, args):
        if len(args) != 1:
            raise WalEvalError(f"str expects 1 argument, got {len(args)}")
        return str(args[0])
    
    # Output operations
    def _builtin_print(self, args):
        """Print arguments concatenated with no separator, then newline"""
        import sys
        output = ''.join(str(arg) for arg in args)
        print(output)
        return None
    
    def _builtin_printf(self, args):
        """Printf-style formatted output"""
        import sys
        if len(args) == 0:
            raise WalEvalError("printf expects at least 1 argument (format string)")
        
        format_str = str(args[0])
        values = args[1:]
        
        # Process escape sequences in the format string
        format_str = format_str.replace('\\n', '\n')
        format_str = format_str.replace('\\t', '\t')
        format_str = format_str.replace('\\r', '\r')
        format_str = format_str.replace('\\\\', '\\')
        
        # Python string formatting with % operator
        try:
            if values:
                # Handle multiple format specifiers
                output = format_str % tuple(values) if len(values) > 1 else format_str % values[0]
            else:
                output = format_str
            sys.stdout.write(output)
            sys.stdout.flush()
        except TypeError as e:
            raise ValueError(f"printf format error: {e}")
        
        return None
