# Giraffe

<img src="https://github.com/hsjunnesson/giraffe/blob/main/giraffe.png" />

This is a benchmark test exploring the difference between gameplay code implemented in C++, Lua, and LuaJIT (with and without just-in-time compilation).
It's a benchmark, not an example of how to code games. It's not meant to be optimized, promise.

This uses [Chocolate](https://github.com/hsjunnesson/chocolate) which is a kind of game framework in OpenGL. It's included as a submodule so be sure to:

```
git submodule init
git submodule update
```

For the cmake dependencies, use something like VCPKG and install the following packages:

- glfw3
- glm
- cjson
- imgui[core,opengl3-binding,glfw-binding]
- backward-cpp

```
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build/
```

This won't work on Mac - it probably will work on Linux.
