[![Build](https://github.com/TxbiG/pyMoss/actions/workflows/build.yml/badge.svg)](https://github.com/TxbiG/pyMoss/actions/workflows/build.yml)
# PyMoss
PyMoss is a `pybind11`  binding project for [Moss Framework](https://github.com/TxbiG/MossFramework).

## Example

```python
import pymoss

with pymoss.Window("PyMoss", 1280, 720) as window:
    while not window.should_close():
        pymoss.poll_events()
```

## Compiling
- Python 3
- C++ 17

## License
The project is distributed under the [MIT license](/LICENSE).