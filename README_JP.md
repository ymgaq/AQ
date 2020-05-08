# 囲碁AI 「GLOBIS-AQZ」

「GLOBIS-AQZ」はDeep Learning技術を利用した囲碁の思考エンジンです。  
日本ルール6目半と中国ルール7目半の両方に対応していることが特徴です。  

このプログラムはGLOBIS-AQZプロジェクトの成果を利用しています。  

> GLOBIS-AQZは、開発：株式会社グロービス、山口祐氏、株式会社トリプルアイズ、開発環境の提供：国立研究開発法人 産業技術総合研究所、協力：公益財団法人日本棋院のメンバーによって取り組んでいる共同プロジェクトです。このプログラムは、GLOBIS-AQZでの試算を活用しています。

オープンソース・ソフトウェアですので、どなたでも無料で使用することができます。  
対局・解析のためのプログラムですので、「[Lizzie](https://github.com/featurecat/lizzie)」「[Sabaki](https://github.com/SabakiHQ/Sabaki)」「[GoGui](https://sourceforge.net/projects/gogui/)」といったGUIソフトに設定して利用してください。  

Please see [here](https://github.com/ymgaq/AQ/blob/master/README.md) for an explanation in English.  
请看[这里的](https://github.com/ymgaq/AQ/blob/master/README_CN.md)中文解释.  

## 1. ダウンロード
[Releases](https://github.com/ymgaq/AQ/releases)からダウンロードしてください。  
Windows 10、 Linuxでビルドした実行ファイルが利用できます。  

それ以外の環境でそのままでは動作しない場合、5.ビルド方法を参考に各環境ごとにビルドを検討してください。(開発者向け)  

## 2. 動作環境
+ OS  : Windows 10, Linux
+ GPU : Nvidia製GPU ([Compute Capability](https://developer.nvidia.com/cuda-gpus) >3.0)
+ [CUDA Toolkit](https://developer.nvidia.com/cuda-toolkit) 10.0 or 10.2
+ [TensorRT 7.0.0](https://docs.nvidia.com/deeplearning/sdk/tensorrt-archived/tensorrt-700/tensorrt-install-guide/index.html)

下記の環境で動作確認をしています。  
+ Ubuntu 18.04 / RTX2080Ti / CUDA10.0 / TensorRT7.0.0
+ Windows 10 Pro (64bit) / RTX2080Ti / CUDA10.2 / TensorRT7.0.0

## 3. 使い方
例えば、日本ルール・コミ6目半で持ち時間20分、切れたら30秒でGTPモードを起動する場合:  
```
$ ./AQ.exe --rule=1 --komi=6.5 --main_time=1200 --byoyomi=30
```
中国ルール・コミ7目半（デフォルト）で探索数（playouts）800固定、ポンダーなしの場合:  
```
$ ./AQ.exe --search_limit=800 --use_ponder=off
```
Tromp-Taylorルール・コミ7目半で15分切れ負けの場合 （[CGOS](http://www.yss-aya.com/cgos/)の設定です）:  
```
$ ./AQ.exe --rule=2 --repetition_rule=2 --main_time=900 --byoyomi=0
```

### 3-1. 環境変数の設定
Windowsの場合、環境変数のPATHに以下のようなパスが登録されている必要があります。  
```
{your_cuda_path}\NVIDIA GPU Computing Toolkit\CUDA\v10.{x}\bin
{your_tensorrt_path}\TensorRT-7.0.0.{xx}\lib
```

### 3-2. エンジンファイルの生成
初回起動時に、UFF（Universal File Format）形式のファイルからお手持ちの環境に最適化されたネットワークエンジンを生成します。  
このエンジン生成には数分程度かかることがあります。  
シリアライズ化されたエンジンファイルが`engine`フォルダに保存されるので、2回目以降はすぐに起動します。  

### 3-3. Lizzieへの登録
engineコマンドにWindowsの場合は `{your_aq_folder}/AQ.exe --lizzie` を登録してください。  
日本ルールで解析させたい場合など、各種設定はAQフォルダ内のconfig.txtを修正してご利用ください。  

## 4. オプション
主なオプションについての説明です。  
コマンドライン引数として指定できる他、config.txtを編集することでも変更可能です。  
`--komi=6.5`のように指定してください。  

### 4-1. 対局オプション
| オプション | デフォルト値 | 説明 |
| :--- | :--- | :--- |
| --num_gpus | 1 | 使用するGPU数です。 |
| --num_threads | 16 | 探索に使用するスレッド数です。 |
| --main_time | 0.0 | 探索の持ち時間（秒）です。 |
| --byoyomi | 3.0 | 秒読みの時間（秒）です。 |
| --rule | 0 | 対局のルールです。 0:中国ルール 1:日本ルール 2:Tromp-Taylorルール |
| --komi | 7.5 | コミ数です。日本ルールの場合は6.5を指定してください。 |
| --batch_size | 8 | 局面の評価を行うバッチ数です。 |
| --search_limit | -1 | 探索回数（playout）。-1で無制限になります。 |
| --node_size | 65536 | 探索の最大ノード数。このノード数に達すると探索を終了します。 |
| --use_ponder | on | 相手の手番で先読みをします。Lizzieで使用するときはonにしてください。 |
| --resign_value | 0.05 | 投了する勝率です。 |
| --save_log | off | 対局の思考ログ・棋譜を保存するかの設定です。 |

### 4-2. 起動モード
主にデバッグ用の機能です。`--lizzie`以外は通常の対局・解析用途には使用しないでください。  
コマンドライン引数としてのみ認識されます。  

| オプション | 起動モード |
| :--- | :--- |
| （指定なし） | GTP通信モード |
| --lizzie | GTP通信に加え、Lizzie用の情報を出力します |
| --self | 自己対局を行います。 `--save_log=on`と併せて使用してください。 |
| --policy_self | ポリシーネットワークの最大の手で自己対局を行います。 |
| --test | 盤面データ構造の整合性などをテストします。 |
| --benchmark | ロールアウトやニューラルネットワークの計算速度を測定します。 |

## 5. ビルド方法
以下は開発者向けの説明になります。  
なお、公開しているソースコードは対局・解析のみの実装で、学習に関する機能は含まれていません。  

AQは、C++11/C++14でコンパイルできるように書かれており、コーディング規約等は概ね以下のページを参考にしています。  
+ コーディング規約: [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)

### 5-1. Linux
Requirements
+ gcc
+ make
+ CUDA Toolkit 10.x
+ TensorRT 7.0.0

Makefile内のCUDA・TensorRTのインクルードパス・ライブラリパスを確認し、makeしてください。

```
make
```

### 5-2. Windows
Requirements
+ Visual Studio 2019 (MSVC v142)
+ CUDA Toolkit 10.x
+ TensorRT 7.0.0

インクルードディレクトリに
```
{your_cuda_path}\NVIDIA GPU Computing Toolkit\CUDA\v10.x\include
{your_tensorrt_path}\TensorRT-7.0.0.xx\include
```

追加のライブラリディレクトリに
```
{your_cuda_path}\NVIDIA GPU Computing Toolkit\CUDA\v10.x\lib\x64
{your_tensorrt_path}\TensorRT-7.0.0.xx\lib
```

追加のライブラリに
```
cudart.lib
nvparsers.lib
nvonnxparser.lib
nvinfer.lib
```

をそれぞれ追加してビルドしてくだい。

## 6. ライセンス
[GPL-3.0](https://github.com/ymgaq/AQ/blob/master/LICENSE.txt)  
開発者: [山口 祐](https://twitter.com/ymg_aq)  
