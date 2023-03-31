## Render Engine for Detection of Reaction Events in Simulations \[REDRES\]

REDRES is a software that allows you to view your molecular dynamics simulations in real-time and validate your event detection data.

The software is supposed to be used in combination with [TOFHET](https://git.rwth-aachen.de/modes/tofhed).
This tool can be used to detect reaction events in your MD simulations, but also provides a python script to 
convert your .xyz- and .extxyz-files to a SQL database, which can be used by REDRES as input.

| [Installation](#Installation) - [Usage](#usage) - [How it works](#how-it-works) - [Releases & Changelogs](#releases--changelogs) - [Demo](#demo) - [Integration](#integration) |

### Installation
For now the software is only available for Linux. 
If you run Ubuntu or a Debian based distribution you can download the software downloading the .deb-package from the packages directory in this repository
and install by running the following commands in your terminal:
```bash
    sudo apt update && sudo apt upgrade
    sudo apt install [path-to-.deb-file]
    git clone
```

### Dependencies:
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