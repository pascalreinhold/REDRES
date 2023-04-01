# Build Instructions
| [Build Tools](#Build Tools) | [External Dependencies](#External Dependencies) | [Compiling](#Compiling) |
## Build Tools
If you want to build the software from source you need to install the following tools, if you don't have them already.
- [git](https://git-scm.com/), to load the git submodules
- [cmake](https://cmake.org/), to build the project
- A C++ Compiler, e.g. the [GNU C++ Compiler](https://gcc.gnu.org/)

If you are using a Debian based distribution like Ubuntu you can install them by running the following commands in your terminal:
```bash
    sudo apt update && apt upgrade
    sudo apt install git # needed to load git submodules
    sudo apt install cmake # C/C++ buildsystem
    sudo apt install build-essential # GNU C++ Compiler and Makefiles
```

## External Dependencies
To following libraries are needed to build the software:
- [vulkan](https://www.khronos.org/vulkan/) - Graphics API
- [glm](https://glm.g-truc.net/0.9.9/index.html) - linear algebra for computer graphics library
- [glfw3](https://www.glfw.org/) - windowing library
- [sqlite3](https://www.sqlite.org/index.html) - SQL engine
- [TBB](https://github.com/oneapi-src/oneTBB) [Optional] - Multi-threading library

If TBB is not installed the software will be build without multi-threading support. Currently, this only affects bond creation.

If you are using a Debian based distribution like Ubuntu you can install them by running the following commands in your terminal:
```bash
    sudo apt update && apt upgrade
    sudo apt install libvulkan-dev vulkan-validationlayers-dev spirv-tools # Vulkan - Graphics API
    sudo apt install libglm-dev # linear algebra for computer graphics library
    sudo apt install libglfw3-dev # window library manages keyboard and mouse input
    sudo apt install libsqlite3-dev # SQL engine
    sudo apt install libtbb-dev # Multi-threading library
```

After installing all the necessary packages you need to initialize the git submodules
by navigating to the root directory of this project and running:

```bash
    git submodule update --init
```

## Compiling
Finally, you can compile the software by running our build script in your terminal (make sure you're in the root directory of this project):

```bash
    chmod +x buildScriptUnix.sh # grant file permission to be executed 
    ./buildScriptUnix.sh # creates a build directory and compiles the software into it 
```