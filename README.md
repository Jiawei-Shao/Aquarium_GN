# WebGLNativePorts
Repository for native ports of WebGL demos, to compare performance with JavaScript versions.  

Steps to build:  
1. git pull https://github.com/Jiawei-Shao/Aquarium_GN.git  
2. gclient sync  
3. gn gen --ide=vs out\Debug (or gn args out\Release)    
4. ninja -C out\Debug Aquarium  
5. Run Aquarium.exe. 
   For example:  
   - out\Debug\Aquarium --num-fish 10000 --backend opengl  
   - out\Debug\Aquarium --num-fish 10000 --backend dawn_d3d12   
