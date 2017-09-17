# AQ

AQ is a program of Go game with level of expert players.  
[CGOS](http://www.yss-aya.com/cgos/19x19/standings.html) rating: 3674 (standings)  

## Requirement
- OS: Linux  
- GPU: Nvidia GPU with [CUDA capability](https://developer.nvidia.com/cuda-gpus) of 6.1  
- CPU: CPU with SSE 4.2  
- CUDA: Cuda-8.0/cudnn-5.x  

## Usage
### Pre-compiled executables
Get them [here](http://github.com/ymgaq/AQ/releases).  

### AQ configuration
Set hardware and time control etc. in 'aq_config.txt.'  

### GoGui setting
command: ./AQ  
working directory: (installed directory)  

## Build from source code.
Build with [bazel](https://bazel.build/) and [TensorFlow](https://www.tensorflow.org/).  
See [this instruction](https://medium.com/jim-fleming/loading-a-tensorflow-graph-with-the-c-api-4caaff88463f).

## License
[MIT](https://github.com/tymgaq/AQ/LICENCE)

## Author
[Yu Yamaguchi](https://twitter.com/ymg_aq)
