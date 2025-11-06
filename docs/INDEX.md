# WAL++ Documentation Index

Welcome to the comprehensive Doxygen documentation for WAL++ (Waveform Analysis Language).

## 📚 Quick Navigation

### Getting Started
- **[API Quick Reference](API_QUICK_REFERENCE.md)** - Start here! Code samples for all major components
- **[Building Documentation](DOXYGEN_GUIDE.md)** - How to generate Doxygen docs
- **[Documentation Status](DOCUMENTATION_STATUS.md)** - Completion checklist and coverage metrics

### Core Components
- **[Core API](@ref Core)** - Main Wal class, REPL, CLI
- **[AST Types](@ref AST)** - Value system, Symbol, WList, Closure, Environment
- **[Evaluator](@ref evaluator)** - S-expression evaluation engine
- **[Operator Registry](@ref operator_registry)** - Extensible operator system
- **[Parser](@ref parser)** - S-expression parsing and grammar
- **[Trace System](@ref trace)** - Waveform loading and signal access

## 🎯 Common Tasks

### I want to...

**Use WAL++ from C++ code**
1. Read [Core API Quick Reference](API_QUICK_REFERENCE.md#core-entry-points)
2. See example in `@ref Wal`
3. Check [Evaluator Examples](API_QUICK_REFERENCE.md#evaluator)

**Understand the architecture**
1. Start with [Core documentation](@ref Core)
2. View Mermaid diagrams in generated HTML
3. Study [Evaluator pipeline](@ref evaluator)
4. Explore [Operator Registry](@ref operator_registry)

**Parse WAL expressions**
1. See [Parser Quick Reference](API_QUICK_REFERENCE.md#parser)
2. Check [Parser documentation](@ref parser)
3. Review grammar examples

**Work with traces and signals**
1. Read [Trace System section](API_QUICK_REFERENCE.md#trace-system)
2. Check [Trace documentation](@ref trace)
3. See signal access patterns

**Build custom operators**
1. Study [Operator Registry](API_QUICK_REFERENCE.md#operator-registry)
2. Review examples in documentation
3. Check batch registration helpers

**Handle errors**
1. See [Error Handling](API_QUICK_REFERENCE.md#error-handling)
2. Review [Errors documentation](@ref Errors)
3. Check exception hierarchy

## 📖 Documentation Files

### Configuration
- **`Doxyfile`** - Main Doxygen configuration (generated from settings below)
- **`doxygen.yaml`** - YAML configuration for readability and tooling

### Guides
- **`docs/DOXYGEN_GUIDE.md`** - Complete guide to Doxygen features and usage
- **`docs/DOCUMENTATION_STATUS.md`** - Completion checklist and statistics
- **`docs/API_QUICK_REFERENCE.md`** - Quick reference with code samples

### Generated (after running doxygen)
- **`docs/doxygen/html/`** - Interactive HTML documentation
- **`docs/doxygen/latex/`** - LaTeX source files
- **`docs/doxygen/refman.pdf`** - PDF documentation
- **`docs/doxygen/xml/`** - XML for tool integration

## 🚀 Building Documentation

### Prerequisites
```bash
brew install doxygen graphviz
npm install -g @mermaid-js/mermaid-cli
```

### Generate
```bash
cd /Users/rmeyer/Projects/walpp
doxygen Doxyfile
```

### View
```bash
open docs/doxygen/html/index.html
```

## 📊 Documentation Coverage

| Component | Documentation | Diagrams | Examples |
|-----------|---------------|----------|----------|
| Core API | ✅ 100% | ✅ | ✅ |
| AST Types | ✅ 100% | ✅ | ✅ |
| Evaluator | ✅ 100% | ✅ | ✅ |
| Operators | ✅ 100% | ✅ | ✅ |
| Parser | ✅ 100% | ✅ | ✅ |
| Trace System | ✅ 100% | ✅ | ✅ |
| Error Handling | ✅ 100% | ✓ | ✅ |

## 🎨 Features

- **Markdown comments** - All documentation in markdown format
- **Mermaid diagrams** - Architecture and data flow visualization
- **Cross-references** - Extensive linking between components
- **Code examples** - Practical usage patterns
- **Multiple formats** - HTML, PDF, XML output
- **Search engine** - Built-in documentation search

## 📋 Module Organization

### @defgroup Core
Main user-facing API
- Wal - Primary entry point
- REPL - Interactive shell
- CLI - Command-line interface

### @defgroup AST
Value types and AST nodes
- Value - Universal value type
- Symbol - Identifiers
- WList - Lists
- Closure - Lambda functions
- Macro - Metaprogramming
- Environment - Scope chains

### @defgroup evaluator
S-expression evaluation
- SEval - Tree-walking evaluator
- Evaluation pipeline
- Environment management

### @defgroup operator_registry
Operator system
- OperatorRegistry - Central registry
- OperatorFunc - Operator signature
- Registration and dispatch

### @defgroup parser
S-expression parsing
- Parser - Recursive descent parser
- Grammar specification
- Tokenization

### @defgroup trace
Waveform system
- Trace - Base class
- TraceContainer - Multi-trace management
- Signal access and routing

## 🔍 Search & Navigation

The generated HTML documentation includes:
- **Search box** - Find any symbol
- **Module browser** - Navigate by component
- **Class hierarchy** - View inheritance
- **File listing** - Browse source
- **Tree view** - Hierarchical navigation

## 💡 Tips

1. **Start with API Quick Reference** - Fastest way to learn
2. **Use search** - Find symbols quickly in generated docs
3. **Follow cross-references** - Click @ref links to explore
4. **Check diagrams** - Visual architecture understanding
5. **Read examples** - Practical usage patterns

## 🐛 Troubleshooting

### "Doxyfile not found"
Run from project root:
```bash
cd /Users/rmeyer/Projects/walpp
doxygen Doxyfile
```

### "Mermaid diagrams not rendering"
Install mermaid-cli:
```bash
npm install -g @mermaid-js/mermaid-cli
```

### "Documentation warnings"
Check the log:
```bash
cat docs/doxygen-warnings.log
```

### "Search not working"
Regenerate with:
```bash
rm -rf docs/doxygen/
doxygen Doxyfile
```

## 📚 Additional Resources

- **[Doxygen Manual](https://doxygen.nl/)** - Official documentation
- **[Markdown Support](https://doxygen.nl/manual/markdown.html)** - Markdown features
- **[Mermaid Diagrams](https://mermaid.js.org/)** - Diagram syntax
- **[Wavedrom](https://wavedrom.com/)** - Waveform diagrams

## 📝 Maintenance

The documentation stays current through:
- Markdown comments directly in source files
- Automatic generation from configuration
- Version control tracking both sources and config
- Continuous integration validation

Update documentation by editing:
- Comment blocks in `.h` files
- Configuration in `doxygen.yaml`
- Guides in `docs/*.md` files

## 🎓 Learning Path

### Beginner
1. Read [API Quick Reference](API_QUICK_REFERENCE.md)
2. Generate docs: `doxygen Doxyfile`
3. Browse HTML docs
4. Try basic examples

### Intermediate
1. Study [Evaluator documentation](@ref evaluator)
2. Explore [Operator Registry](@ref operator_registry)
3. Read [DOXYGEN_GUIDE.md](DOXYGEN_GUIDE.md)
4. Implement custom operators

### Advanced
1. Deep dive into source comments
2. Understand parser internals
3. Study trace system architecture
4. Explore CLI and REPL implementation

---

**Documentation Generated**: 2025-11-06  
**Version**: 1.0  
**Status**: ✅ Complete (100% coverage)

**For the latest version, always build with:** `doxygen Doxyfile`
