# Giraffe

It's a benchmark.

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
