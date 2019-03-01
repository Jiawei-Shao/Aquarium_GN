# WebGLNativePorts
Repository for native ports of WebGL demos, to compare performance with JavaScript versions.

Steps to build:
git pull https://github.com/Jiawei-Shao/Aquarium_GN.git
gclient sync
gn gen --ide=vs out\Debug
ninja -C out\Debug Aquarium
