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
../configure --prefix=/usr/local
make install
```
