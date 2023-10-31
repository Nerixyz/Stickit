# <img src="src/icon.ico" height="20" align="center"> Stickit

Stickit is a small Windows tool that displays an image above all other windows. The image is the currently copied image, making it easy to use screenshots (<kbd>WIN</kbd> + <kbd>SHIFT</kbd> + <kbd>S</kbd>).

Download the app or the installer from the [releases tab](https://github.com/Nerixyz/stickit/releases).

## Building

A recent C++ compiler and CMake are required to build. This example uses `Ninja` as the generator.

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cd build
ninja all
```
