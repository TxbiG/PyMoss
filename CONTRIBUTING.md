# Contributing to PyMoss

Thank you for contributing to **PyMoss**, the Python binding for Moss Framework, implemented with pybind11 and designed to make Moss's C++ game-development functionality accessible from Python.

## Before You Start

For public API, ownership, native interop, or packaging changes, open an issue first when the change is substantial.

Please search existing issues and pull requests before starting work.

## Development Requirements

- Git
- Python 3
- CMake
- C++17
- pybind11
- A local Moss Framework checkout

## Building

```bash
git clone https://github.com/TxbiG/PyMoss.git
cd PyMoss

cmake -S . -B build
cmake --build build
```

The repository also contains Python package metadata in `pyproject.toml`. Changes affecting packaging should be tested through the package workflow, not only a native CMake build.

## Repository Structure

- `python/pymoss/` — Python-facing package/binding code
- `src/` — native implementation
- `docs/` — documentation
- `examples/` — examples
- `performance/` — benchmarks
- `build/workflows/` — CI/build configuration
- `pyproject.toml` — Python package metadata/configuration

## Areas for Contribution

- Python API coverage
- pybind11 bindings
- Pythonic wrappers/helpers
- Type conversion
- Resource ownership/lifetime
- Error handling
- Packaging and distributions
- Performance
- Examples
- Documentation
- Tests
- Cross-platform builds

## Python API Guidelines

Bindings should feel natural to Python users without hiding important Moss semantics.

Prefer:

- Clear Python naming
- Useful exceptions
- Predictable conversions
- Safe lifetime management
- Python-friendly containers where they do not create unnecessary copying
- Context-manager support where it makes resource ownership clearer

Avoid silently copying large buffers, textures, audio data, or other expensive resources.

## Ownership and Lifetime

For every native resource exposed to Python, make ownership explicit:

- Define who owns the resource.
- Define when it is destroyed.
- Avoid dangling references.
- Ensure Python objects cannot outlive required native resources.
- Document non-obvious lifetime rules.

## Testing

Test:

- Normal API calls
- Invalid arguments
- Python type conversions
- Resource creation/destruction
- Exceptions
- Repeated calls
- Platform-specific behaviour
- Package/import behaviour where relevant

For binding bugs, add a small Python regression test whenever practical.

## Packaging

Changes affecting `pyproject.toml`, package layout, native libraries, or wheel generation should be tested using the actual package workflow.

A successful local CMake build does not by itself prove that the Python distribution is installable or publishable.

## Commit Messages

Recommended prefixes:

```text
feat: expose Moss renderer resources
fix: prevent dangling Python texture references
docs: add PyMoss getting started guide
test: cover vector conversions
perf: reduce binding allocations
build: improve pybind11 discovery
```

## Pull Requests

Include:

- What changed
- Python usage examples where useful
- Tests performed
- Python version
- Platform/toolchain
- Packaging impact
- API compatibility notes

## Licence

PyMoss is distributed under the **MIT License**. Contributions should be compatible with the repository's licence and applicable pybind11/Moss/third-party licence requirements.

Thank you for contributing.
