# Windows instructions

## Build instructions

### Requirements:

- Microsoft Visual Studio 2022 with C++ support: https://visualstudio.microsoft.com/downloads/ (Community Edition is free)

### Build steps:

- open a new Visual Studio Developer terminal: Start -> Visual Studio 2022 -> Developer Command Prompt for VS22
- in that terminal run these commands:
```
cd signal-generator-windows
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
cd ..
```

The signal generator executable will be under build\Release\signal-generator.exe


## How to run it

TODO
