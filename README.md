# AQ

AQ is a program of Go game with level of top players.  
[CGOS](http://www.yss-aya.com/cgos/19x19/standings.html) rating: 3952 ([BayesElo](http://www.yss-aya.com/cgos/19x19/bayes.html))  

## Requirement
### Windows
#### AQ (with GPU)
- OS: 64-bit Windows 7 or later (NOT tested on 8.1/10)  
- GPU: Nvidia GPU with [CUDA capability](https://developer.nvidia.com/cuda-gpus) of >=3.0  
- CPU: CPU with SSE 4.2  

#### AQ-mini (with CPU only)
- OS: 64-bit Windows 7 or later (NOT tested on 8.1/10)  
- CPU: CPU with SSE 4.2  

### Linux
- OS: 64-bit Linux  
- GPU: Nvidia GPU with [CUDA capability](https://developer.nvidia.com/cuda-gpus) of >=3.0  
- CPU: CPU with SSE 4.2  

## Usage
### Pre-compiled executables
Get them [here](http://github.com/ymgaq/AQ/releases).  

### AQ configuration
Set hardware and time control etc. in 'aq_config.txt.'  
#### Time control on CGOS
First of all, it is not recommended that users connect the released version as it is to CGOS. The developer is well aware of the capabilities of the released versions and [tested it in advance](http://www.yss-aya.com/cgos/19x19/cross/AQ-2.1.1-4t1g.html) with `4thread/1GPU: i7-6700/GTX1080`.  
Of course, if you made your own changes to the source code or pb files, that is welcome. Please check your AQ's rating and send pull request!  
  
The recommended settings are as follows.  

```
-main time[sec] =900  
-byoyomi[sec] =0  
-emergency time[sec] =15 #set 60 when connecting from outside Japan  
```

!!!Caution!!! This version was trained in Komi = 6.5 for the Japanese rule. So, AQ often loses 0.5 point at Black, but that is inevitable.

### GoGui setting
[GoGui](https://sourceforge.net/projects/gogui/files/gogui/1.4.9/) is a graphical interface to Go-engines (programs without own GUI), which use the Go Text Protocol (GTP).  
See the 'GTP Shell' console to know AQ's thinking log.  
#### Linux
command: `(install directory)/AQ`  

```
(Ex.)  
/home/user/gogui-1.4.9/AQ/AQ  
```

#### Windows
command: `(install directory)\AQ.exe`  

```
(Ex.)  
C:\Users\user\gogui-1.4.9\AQ\AQ.exe  
```

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
