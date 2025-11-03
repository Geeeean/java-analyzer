# Project Setup and Build Instructions

This project has been developed and tested on POSIX-compliant systems only (Linux and macOS).
Compatibility with Windows is not guaranteed.

## Prerequisites

- `gcc`

The code has been tested with GCC 14 https://gcc.gnu.org/gcc-14/ feel free to use another one by specifying it explicitly in the Makefile.

## Installing Dependencies

Before building the project, install the required external libraries.  
From the project root directory:

```sh
chmod +x install_deps.sh
./install_deps.sh
```

This script will download, compile, and place the necessary files in the `lib` directory.

# Building the Project
```sh
make
```

If the build completes successfully, the compiled binaries will be located in the `bin` directory.

# Running the project
Create a file named `java-analyzer.conf` in `$HOME/config/java-analyzer`and specify some configurations options.
The file should look like this:
```
name                  a_cool_name.c
version               0.0
group                 a_cooler_name
for_science           0
tags                  static,c
jpamb_source_path     .../jpamb/src/main/java
jpamb_decompiled_path .../jpamb/decompiled
```

Be sure to specify the correct path for the JPAMB benchmark suite: https://github.com/kalhauge/jpamb.

To run it from the root dir execute:
```sh
./bin/analyzer
```

# References
https://www.sciencedirect.com/science/article/pii/S0164121221001394
https://arxiv.org/abs/2009.05865
https://arxiv.org/abs/1909.05951


