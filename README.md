### Forked from the blitz-research/blitz3d_soloud.

### Building Blitz3D from source on Windows

You will need to install Microsoft Visual Studio, and the CMake and Git utilities. Any recent version of MSVC should be OK, I am currently using Community Edition 2022.

You will also need to install the following optional MSVC components: "Desktop development with C++", "MFC and ATL support" and "ASP.NET and web development".

Then, from a DOS prompt:

``` shell
git clone https://github.com/blitz-research/blitz3d_soloud.git
cd blitz3d_soloud
cmake -S . -B cmake-build-release -A Win32 -G "Visual Studio 17 2022"
cmake --build cmake-build-release --config Release
```
Assuming all went well, the BLITZ3D_INSTALL directory will contain the final binaries, simply run Blitz3D.exe to get blitzing!

### Building Blitz3D from source on MacOS
... Coming soon.


### Building Blitz3D from source on Linux
... Coming soon.


### Building Blitz3D from source on ARM
... Coming soon.


### Too lazy to build?

No releases yet...
