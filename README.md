# AQ

AQ is a program of Go game with level of expert players.  
[CGOS](http://www.yss-aya.com/cgos/19x19/standings.html) rating: 3746 (standings)  

## Requirement
### Windows
#### AQ (with GPU)
- OS: 64-bit Windows 7 or later (NOT tested on 8.1/10)  
- GPU: Nvidia GPU with [CUDA capability](https://developer.nvidia.com/cuda-gpus) of 3.0/3.5/5.2/6.1
- CPU: CPU with SSE 4.2  
#### AQ-mini (with CPU only)
- OS: 64-bit Windows 7 or later (NOT tested on 8.1/10)  
- CPU: CPU with SSE 4.2  
### Linux
- OS: 64-bit Linux  
- GPU: Nvidia GPU with [CUDA capability](https://developer.nvidia.com/cuda-gpus) of 3.0/3.5/5.2/6.1  
- CPU: CPU with SSE 4.2  
- CUDA: CUDA 8.0 and cudnn 5.1  

## Usage
### Pre-compiled executables
Get them [here](http://github.com/ymgaq/AQ/releases).  

### AQ configuration
Set hardware and time control etc. in 'aq_config.txt.'  

### GoGui setting
[Gogui](https://sourceforge.net/projects/gogui/files/gogui/1.4.9/) is a graphical interface to Go-engines (programs without own GUI), which use the Go Text Protocol (GTP).  
See the 'GTP Shell' console to know AQ's thinking log.  
#### Linux
command: (install directory)/AQ  
working directory: (install directory)  
(ex. /home/user/gogui-1.4.9/AQ/AQ  
     /home/user/gogui-1.4.9/AQ   )  
#### Windows
command: (install directory)\\AQ.exe  
working directory: (install directory)  
(ex. C:\\User\\user\\gogui-1.4.9\\AQ\\AQ.exe  
     C:\\User\\user\\gogui-1.4.9\\AQ        )  

## Build from source code
### Linux
Build with [bazel](https://bazel.build/) and [TensorFlow](https://www.tensorflow.org/).  
See [this instruction](https://medium.com/jim-fleming/loading-a-tensorflow-graph-with-the-c-api-4caaff88463f).  
### Windows
Build with [cmake](https://cmake.org/) and [TensorFlow](https://www.tensorflow.org/).  
See [this instruction](https://joe-antognini.github.io/machine-learning/windows-tf-project).  

## License
[MIT](https://github.com/ymgaq/AQ/blob/master/LICENSE.txt)

## Author
[Yu Yamaguchi](https://twitter.com/ymg_aq)
