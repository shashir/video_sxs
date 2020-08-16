# Video side by side

Plays videos side by side in order to compare quality.

## Dependencies

* CMake
* OpenCV

```sh
brew install cmake
brew install opencv
```

## Build and run

```sh
mkdir build
cd build
cmake ..
cmake --build .
./video_sxs --help
./video_sxs --input1=/path/to/a.mp4 --input2=/path/to/b.mp4 --output=/path/to/output.mp4
```
