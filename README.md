# README

## Dependencies:
This piece of software has a lot of external dependencies. Before installing them make sure all your packages are up-to-date:
```
    sudo apt update && sudo apt upgrade
```
Now install the packages for git, cmake, vulkan, glm, glfw3, sqlite3 (probably you already have most of them installed already): 
```bash
    sudo apt install git # needed to load git submodules
    sudo apt install cmake # part of the buildsystem for the project
    sudo apt install build-essential # GNU C++ Compiler and Makefiles
    sudo apt install sqlite3 # SQL engine
    sudo apt install libvulkan-dev vulkan-validationlayers-dev spirv-tools # vulkan - Graphics API
    sudo apt install libglm-dev # linear algebra for computer graphics library
    sudo apt install libglfw3-dev # window library manages keyboard and mouse input
```
If you installed all necessary packages you got it 
```bash
    git submodule update --init
```

## Building:
To build the program run the build script in this directory
```bash
    chmod +x buildScriptUnix.sh # grant file permission to be executed 
    ./buildScriptUnix.sh
```