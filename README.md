# Hot-reloader
(more like build-execute automator)

Software development often involves running the same commands over and over. Boring!

`hot-reload` is a simple, standalone tool that watches a path and compiles your code and re-runs it everytime it detects a modification.

Hugely inspired by [watchexec](https://github.com/watchexec/watchexec)

# Usage

```sh
hot-reload <path to watch> <build script> <target binary>
```

# Building

In the project directory:
```sh
mkdir -p build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make
sudo make install
```

For debug builds:
```sh
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
```
