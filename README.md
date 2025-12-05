# untitled galaxy simulator

A galaxy simulation designed for visual beauty rather than scientific realism.

![alt text](ghassets/image.png)
![alt text](ghassets/image2.png)
![alt text](ghassets/image3.png)

Watch YouTube video:

[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/P6jMlMi1iTk/0.jpg)](https://www.youtube.com/watch?v=P6jMlMi1iTk)

## Requirements

### Software
- **Visual Studio 2022** (v143 platform toolset) OR **VSCode** with C++ tools
- **CMake** (optional, for VSCode/CLI build)
- **Windows SDK 10.0** or later
- **C++17** compiler support

### Dependencies
Dependencies (GLFW 3.4, GLAD) are managed locally in the `libs/` folder.

**First Step:** Run the setup script to download all dependencies automatically.
```powershell
.\setup_libs.ps1
```

## Build

### Option 1: Visual Studio 2022
1.  Ensure you have run `.\setup_libs.ps1`.
2.  Open `Space C++.sln` in Visual Studio 2022.
3.  Select **x64** platform.
4.  Choose **Release** configuration.
5.  Build and run (F5).

### Option 2: CMake (VSCode / CLI)
1.  Ensure you have run `.\setup_libs.ps1`.
2.  Create a build directory:
    ```powershell
    mkdir build
    cd build
    ```
3.  Configure and Build:
    ```powershell
    cmake ..
    cmake --build . --config Release
    ```
4.  Run:
    ```powershell
    .\Release\galaxy-sim.exe
    ```

## Configuration

The simulation can be configured through the UI (press TAB to toggle) or by modifying `createDefaultGalaxyConfig()` in `main.cpp`:

```cpp
config.numStars = 1000000;
config.spiralTightness = 0.3;
config.diskRadius = 800.0;
config.bulgeRadius = 150.0;
config.rotationSpeed = 1.0;
```

## Controls

- **WASD** - Move camera (Noclip style - fly in direction of view)
- **Space** - Move Up (World vertical)
- **CTRL** - Move Down (World vertical)
- **Shift** - Speed up
- **Mouse** - Look around
- **Scroll** - Zoom in/out
- **Ctrl** (hold) - Move zoom anchor to solar system instead of (0,0,0)
- **Tab** - Toggle simulation config UI

## Platform Support
- **Windows** ✅
- **Linux** 🖕
- **MacOS** 🫰❌0️⃣0️⃣0️⃣🫵😂 [download](https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcSziL_UbSIwkRr35AS4NW_Y6LsV4_Svlp4u_w&s)

## License
no license do whatever you want
