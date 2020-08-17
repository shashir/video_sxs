# Video side by side

Plays videos side by side in order to compare quality.

## Dependencies

* CMake
* OpenCV
* Abseil

```sh
brew install cmake
brew install opencv

cd path/to/video_sxs
git clone https://github.com/abseil/abseil-cpp.git
```

## Build and run

```sh
cd path/to/video_sxs
mkdir build
cd build
cmake ..
cmake --build .
./video_sxs --help
./video_sxs --input1=/path/to/a.mp4 --input2=/path/to/b.mp4 --output=/path/to/output.mp4
```
