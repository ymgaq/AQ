# 顾彼思问鼎围棋 (GLOBIS-AQZ)

顾彼思问鼎围棋(GLOBIS-AQZ)是一个使用深度学习技术的围棋引擎。  
它的特点是既支持日本规则，也支持中国规则。  

该项目利用GLOBIS-AQZ项目的结果。  

> GLOBIS-AQZ is a joint project developed by GLOBIS Corporation, Mr. Yu Yamaguchi, and Tripleize Co., Ltd., provided by the National Institute of Advanced Industrial Science and Technology (AIST), and cooperated by the Nihon Ki-in. This program uses the result of GLOBIS-AQZ.

由于它是开源软件，任何人都可以免费使用。  
本程序是用来玩游戏和分析游戏的，请将其设置为[Lizzie](https://github.com/featurecat/lizzie)、[Sabaki](https://github.com/SabakiHQ/Sabaki)、[GoGui](https://sourceforge.net/projects/gogui/)等GUI软件。  

请注意，此描述为机器翻译，因此可能存在不准确的地方。

Please see [here](https://github.com/ymgaq/AQ/blob/master/README.md) for an explanation in English.  
日本語の説明は[こちら](https://github.com/ymgaq/AQ/blob/master/README_JP.md)をご覧ください。  

## 1. 下载
从[Releases](https://github.com/ymgaq/AQ/releases)中下载.  
Windows 10和Linux（Ubuntu 18.04）上构建的可执行文件。  

如果它在其他环境中无法正常运行，请考虑为每个环境构建它（针对开发者）。  

## 2. 动作环境要求
+ OS  : Windows 10, Linux
+ GPU : Nvidia's GPU ([Compute Capability](https://developer.nvidia.com/cuda-gpus) >3.0)
+ [CUDA Toolkit](https://developer.nvidia.com/cuda-toolkit) 10.0 or 10.2
+ [TensorRT 7.0.0](https://docs.nvidia.com/deeplearning/sdk/tensorrt-archived/tensorrt-700/tensorrt-install-guide/index.html)

它在以下环境中进行了测试：  
+ Ubuntu 18.04 / RTX2080Ti / CUDA10.0 / TensorRT7.0.0
+ Windows 10 Pro (64bit) / RTX2080Ti / CUDA10.2 / TensorRT7.0.0

## 3. 如何使用
例如，如果你想在日本规则的情况下启动GTP模式，时间设置为20分钟和30秒:  
```
$ ./AQ.exe --rule=1 --komi=6.5 --main_time=1200 --byoyomi=30
```
在中国规则下（默认），搜索（playouts）的数量固定为800，没有pondering:  
```
$ ./AQ.exe --search_limit=800 --use_ponder=off
```

### 3-1. 设置环境变量
在Windows的情况下，必须在PATH环境变量中注册以下路径。  
```
{your_cuda_path}\NVIDIA GPU Computing Toolkit\CUDA\v10.{x}\bin
{your_tensorrt_path}\TensorRT-7.0.0.{xx}\lib
```

### 3-2. 生成引擎文件
第一次启动时，它会从UFF（Universal File Format）格式的文件中生成一个为您的环境优化的网络引擎。  
可能需要几分钟的时间来生成这个引擎。  
序列化的引擎文件被保存在`engine`文件夹中，所以它将会立即启动第二次。  

### 3-3. 向Lizzie注册
对于Windows，在引擎命令中添加`{your_aq_folder}/AQ.exe --lizzie`。  
例如，如果你想用日本规则分析，请修改AQ文件夹中的config.txt文件，使用各种设置。  

## 4. 选项
以下是对主要选项的描述。  
它可以作为命令行参数指定，也可以通过编辑config.txt来改变。  
例如，`--komi=6.5`。  

### 4-1. 游戏选项
| 选项 | 缺省 | 描述 |
| :--- | :--- | :--- |
| --num_gpus | 1 | 要使用的GPU数量。 |
| --num_threads | 16 | 用于搜索的线程数量。 |
| --main_time | 0.0 | 搜索的主要时间（单位：秒）。 |
| --byoyomi | 3.0 | 倒计时时间（单位：秒）。 |
| --rule | 0 | 的游戏规则。 0：中国规则 1：日本规则 2：Tromp-Traylor规则 |
| --komi | 7.5 | Komi的数量。在日本规定的情况下，请注明6.5。 |
| --batch_size | 8 | 一次评价的批次数。 |
| --search_limit | -1 | 搜索次数（播放次数）。-1表示该选项被禁用。 |
| --node_size | 65536 | 搜索的最大节点数。当达到这个节点数时，搜索结束。 |
| --use_ponder | on | 是否要在对手的回合中提前阅读。 在Lizzie中使用时必须打开它。 |
| --resign_value | 0.05 | 放弃中奖率。 |
| --save_log | off | 是否保存游戏中的思想记录和sgf文件。 |

### 4-2. 启动模式
主要用于调试。请不要使用`--lizzie`以外的任何其他游戏进行正常的游戏和分析。  
T它们只被认可为命令行参数。  

| 选项 | 启动模式 |
| :--- | :--- |
| (不详) | GTP通信模式 |
| --lizzie | 除了GTP通讯外，它还能为Lizzie输出信息。 |
| --self | 它启动了一个自我游戏。 用`--save_log=on`来使用它。 |
| --policy_self | 它以政策网络中的绝招开始了一场自我游戏。 |
| --test | 测试板式数据结构的一致性等。 |
| --benchmark | 它可以衡量推出和神经网络的计算速度。 |

## 5. 汇编方法
以下是对开发者的解释。  
源代码只实现了游戏和分析，不包含任何学习功能。  

AQ的编写是为了能用C++11/C++14进行编译，编码约定一般参考下面的页面。  
+ [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)

### 5-1. Linux
Requirements
+ gcc
+ make
+ CUDA Toolkit 10.x
+ TensorRT 7.0.0

在Makefile中检查CUDA和TensorRT的include路径和库路径。  

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

将上述内容一一添加，并建立起来。  

## 6. License
[GPL-3.0](https://github.com/ymgaq/AQ/blob/master/LICENSE.txt)  
作者: [山口 祐](https://twitter.com/ymg_aq)  
