# GLOBIS-AQZ

GLOBIS-AQZ is a Go game engine that uses Deep Learning technology.  
It features support for both the Japanese rule with Komi 6.5 and the Chinese rule with Komi 7.5.  

This program utilizes the results of the GLOBIS-AQZ project.  

> GLOBIS-AQZ is a joint project developed by GLOBIS Corporation, Mr. Yu Yamaguchi, and Tripleize Co., Ltd., provided by the National Institute of Advanced Industrial Science and Technology (AIST), and cooperated by the Nihon Ki-in. This program uses the result of GLOBIS-AQZ.

Since it is open source software, anyone can use it for free.  
This program is for playing and analyzing games, so please set it to GUI software such as [Lizzie](https://github.com/featurecat/lizzie), [Sabaki](https://github.com/SabakiHQ/Sabaki) and [GoGui](https://sourceforge.net/projects/gogui/).  

日本語の説明は[こちら](https://github.com/ymgaq/AQ/blob/master/README_JP.md)をご覧ください。  
请看[这里的](https://github.com/ymgaq/AQ/blob/master/README_CN.md)中文解释.  

## 1. Downloads
Download executable files from [Releases](https://github.com/ymgaq/AQ/releases).  
The executable files built on Windows 10 or Linux (Ubuntu 18.04) are available.  

If it does not work as it is in other environments, please consider building it for each environment. (for developers)  

## 2. Requirements
+ OS  : Windows 10, Linux (64-bit)
+ GPU : Nvidia's GPU ([Compute Capability](https://developer.nvidia.com/cuda-gpus) >3.0)
+ [CUDA Toolkit](https://developer.nvidia.com/cuda-toolkit) 10.0 or 10.2
+ [TensorRT 7.0.0](https://docs.nvidia.com/deeplearning/sdk/tensorrt-archived/tensorrt-700/tensorrt-install-guide/index.html)

It has been tested in the following environment.  
+ Ubuntu 18.04 / RTX2080Ti / CUDA10.0 / TensorRT7.0.0
+ Windows 10 Pro (64bit) / RTX2080Ti / CUDA10.2 / TensorRT7.0.0

## 3. How to use
For example, if you want to start GTP mode in the case of Japanese rule and with time settings of 20 minutes and 30-seconds byoyomi:  
```
$ ./AQ.exe --rule=1 --komi=6.5 --main_time=1200 --byoyomi=30
```
With Chinese rule and Komi 7.5 (default), the number of searches (playouts) is fixed at 800 without ponder:  
```
$ ./AQ.exe --search_limit=800 --use_ponder=off
```
With Tromp-Taylor rule and Komi 7.5, 15 minutes sudden death such as games on [CGOS](http://www.yss-aya.com/cgos/):  
```
$ ./AQ.exe --rule=2 --repetition_rule=2 --main_time=900 --byoyomi=0
```

### 3-1. Setting environment variables
In the case of Windows, the following path must be registered in the PATH environment variable.  
```
{your_cuda_path}\NVIDIA GPU Computing Toolkit\CUDA\v10.{x}\bin
{your_tensorrt_path}\TensorRT-7.0.0.{xx}\lib
```

### 3-2. Generating engine files
The first time it starts up, it generates a network engine optimized for your environment from a file in UFF (Universal File Format) format.  
It may take a few minutes to generate this engine.  
The serialized engine files are saved in the `engine` folder, so it will start immediately the second time around.  

### 3-3. Register with Lizzie
For Windows, add `{your_aq_folder}/AQ.exe --lizzie` to the engine command.  
For example, if you want to analyze by Japanese rules, please modify the config.txt file in the AQ folder to use various settings.  

## 4. Options
Here's a description of the main options.  
It can be specified as a command line argument, or it can be changed by editing config.txt.  
For example, `--komi=6.5`.  

### 4-1. Game options
| Option | default | description |
| :--- | :--- | :--- |
| --num_gpus | 1 | The number of GPUs to use. |
| --num_threads | 16 | The number of threads to be used for searching. |
| --main_time | 0.0 | Main time of search (in seconds). |
| --byoyomi | 3.0 | Byoyomi (in seconds). |
| --rule | 0 | The rule of the game. 0: Chinese rule 1: Japanese rule 2: Tromp-Taylor rule |
| --komi | 7.5 | Number of Komi. In the case of Japanese rule, please specify 6.5. |
| --batch_size | 8 | The number of batches for a single evaluation. |
| --search_limit | -1 | The number of searches (playouts). -1 means this option is disable. |
| --node_size | 65536 | Maximum number of nodes of the search. When this number of nodes is reached, the search is terminated. |
| --use_ponder | on | Whether or not to read ahead in the opponent's turn. You must turn it on when using it in Lizzie. |
| --resign_value | 0.05 | the winning rate to be given up. |
| --save_log | off | Whether or not to save the game's thought logs and sgf files. |

### 4-2. Launch modes
Mainly for debugging. Please do not use any other games other than `--lizzie` for normal games and analysis.  
They are only recognized as a command line argument.  

| Option | Launch mode |
| :--- | :--- |
| (not specified) | GTP communication mode |
| --lizzie | In addition to GTP communication, it outputs information for Lizzie. |
| --self | AQ starts a self game. |
| --policy_self | AQ starts a self game with the best move in policy networks. |
| --test | Tests the consistency of the board data structure, etc. |
| --benchmark | Measures the computational speed of rollouts and neural networks. |

## 5. Compilation method
The following is an explanation for developers.  
The source code is implemented only for games and analysis, and does not include any learning functions.  

AQ is written so that it can be compiled with C++11/C++14, and the coding conventions are generally referred to the following page.  
+ [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)

### 5-1. Linux
Requirements
+ gcc
+ make
+ CUDA Toolkit 10.x
+ TensorRT 7.0.0

Check the include path and library path of CUDA and TensorRT in the Makefile and make it.  

```
$ make
```

### 5-2. Windows
Requirements
+ Visual Studio 2019 (MSVC v142)
+ CUDA Toolkit 10.x
+ TensorRT 7.0.0

Additional include directories:
```
{your_cuda_path}\NVIDIA GPU Computing Toolkit\CUDA\v10.x\include
{your_tensorrt_path}\TensorRT-7.0.0.xx\include
```

Additional library directories:
```
{your_cuda_path}\NVIDIA GPU Computing Toolkit\CUDA\v10.x\lib\x64
{your_tensorrt_path}\TensorRT-7.0.0.xx\lib
```

Additional library files:
```
cudart.lib
nvparsers.lib
nvonnxparser.lib
nvinfer.lib
```

Add each of the above and build it.  

## 6. License
[GPLv3](https://github.com/ymgaq/AQ/blob/master/LICENSE.txt)  
Author: [Yu Yamaguchi](https://twitter.com/ymg_aq)  
