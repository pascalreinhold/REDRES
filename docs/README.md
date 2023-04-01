## Render Engine for Detection of Reaction Events in Simulations \[REDRES\]

REDRES is a software that allows you to view your molecular dynamics simulations in real-time and validate your event detection data.

The software is supposed to be used in combination with [TOFHET](https://git.rwth-aachen.de/modes/tofhed).
This tool can be used to detect reaction events in your MD simulations, but also provides a python script to 
convert your .xyz- and .extxyz-files to a SQL database, which can be used by REDRES as input.

The software is still in active development, if you encounter any bugs or have feature suggestions
file a GitHub issue or contact me directly at pascalreinhold42@gmail.com.

| [Installation](#Installation) - [Usage](#usage) |

### Installation
For now the software is only available for Linux. 
If you run a Debian based distribution like Ubuntu you can just grab the .deb-package from the packages directory and install by via your package manager, by
running something like:
```bash
    sudo apt update && sudo apt upgrade
    sudo apt install /filepath_to_package/packe_name.deb
```
If you are using a different distribution 
or want to build from source for other reasons read the [build.md](docs/build.md) in the /docs-directory.

### Usage
After installing the software you can start it by running the following command in your terminal:
```bash
    gpu_driven_rcc # start without loading a database
    gpu_driven_rcc <path-to-your-database.db> # start + load database
    gpu_driven_rcc <path-to-your-database.db> <relative-path-from-executable-to-asset-directory> # start + load database + specify asset directory
```
The third option is only necessary if you build the software from source or
if the assets directory is not in the default location (/usr/share/gpu_driven_rcc)
for other reasons.
If you build from source following the instruction in [build.md](docs/build.md),
the second command line should be "../"  