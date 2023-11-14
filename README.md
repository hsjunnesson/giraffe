# Giraffe

<img src="https://github.com/hsjunnesson/giraffe/blob/main/giraffe.png" />

This is a benchmark test exploring the difference between gameplay code implemented in C++, Lua, and LuaJIT (with and without just-in-time compilation), LuaU, and Angelscript.
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

If you set the `SCRIPT` property you can select a scripting language, instead of the C++ reference implementation. Available options are `LUA51` (plain vanilla Lua 5.1), `LUAJIT`, `LUAU`, or `ANGELSCRIPT`.

For `LUAJIT`, you will need to have PkgConfig installed to manage the dependency.

Example make and build:

```
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake -DSCRIPT=LUA51
cmake --build build/
```
