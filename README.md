# Project Setup and Build Instructions

This project has been developed and tested on POSIX-compliant systems only (Linux and macOS).
Compatibility with Windows is not guaranteed.

## Prerequisites

- `gcc` version 14

Ensure your system compiler is set to use GCC 14, or specify it explicitly in your build environment.

## Installing Dependencies

Before building the project, install the required external libraries.  
From the project root directory:

```sh
chmod +x install_deps.sh
./install_deps.sh
```

This script will download, compile, and place the necessary files in the lib/ directory.

# Building the Project
```sh
make
```

If the build completes successfully, the compiled binaries will be located in the `bin` directory.

# References
https://www.sciencedirect.com/science/article/pii/S0164121221001394
https://arxiv.org/abs/2009.05865
https://arxiv.org/abs/1909.05951


