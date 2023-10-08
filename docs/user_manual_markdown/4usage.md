# Usage

After installing the software you can start it by running the following command in your terminal:

```bash
    gpu_driven_rcc # start without loading a database
    gpu_driven_rcc <path-to-your-database.db> # start + load database
    gpu_driven_rcc <path-to-your-database.db> <relative-path-from-executable-to-asset-directory> # start + load database + specify asset directory
```

The third option is only necessary if you build the software from source or
if the assets directory is not in the default location (/usr/share/gpu_driven_rcc)
for other reasons.
If you build from source following the [Build Instructions](3build.md#build-instructions),
the second command line should be "../"  