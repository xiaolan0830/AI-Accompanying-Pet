# 基于 RT-Thread 的多模态交互 AI 电子宠物

本项目为嵌入式大赛参赛作品，实现一只具备**语音对话**与**表情识别**能力的桌面 AI 宠物。系统采用 **STM32 + ESP32 + Mac** 协同架构：STM32 基于 **RT-Thread** 负责舵机动作与模块间调度；ESP32 负责屏幕表情与语音采集/播放；Mac 端运行 Python 软件，完成情绪识别与大语言模型语音交互。

---

## 系统架构

```text
                    ┌─────────────────────────────────────┐
                    │           Mac（Python）              │
                    │  情绪识别 │ Whisper │ DeepSeek │ TTS │
                    └──────┬──────────────┬───────────────┘
                           │ HTTP/UDP     │ UDP :8888
              ┌────────────┼──────────────┼────────────┐
              │            │              │            │
       ESP32-CAM      UDP 情绪      PCM 上下行    （热点局域网）
              │            │              │            │
              └────────────┼──────────────┼────────────┘
                           │ UART         │
                    ┌──────┴──────────────┴──────┐
                    │         ESP32-S3            │
                    │  ST7789 屏幕 │ INMP441 麦克风 │
                    │  MAX98357A 喇叭 │ WiFi UDP   │
                    └──────────────┬───────────────┘
                                   │ UART
                    ┌──────────────┴───────────────┐
                    │      STM32（RT-Thread）       │
                    │  5 路舵机 │ SU-03T 串口通信   │
                    └──────────────┬───────────────┘
                                   │ UART
                              ┌────┴────┐
                              │ SU-03T  │
                              │ 离线唤醒 │
                              └─────────┘
```

### 典型交互流程

1. 用户说唤醒词「小宠小宠」→ **SU-03T** 本地回复「在的」
2. **STM32** 收到 SU-03T 指令后，通过串口向 **ESP32** 发送 `WAKE`，并驱动舵机做唤醒动作
3. **ESP32** 进入 HAPPY 表情、开始 **INMP441** 录音，经 WiFi **UDP** 将 PCM 发往 Mac
4. **Mac** 检测静默后完成 STT → **DeepSeek** 生成回复 → **edge-tts** 合成语音
5. Mac 发送 `TTS_START` → PCM 分片 → `TTS_END`；**ESP32** 停录、缓存并驱动喇叭播放，屏幕切换 SPEAK 动画
6. 并行地，**Mac** 从 ESP32-CAM 取图做表情识别，经 UDP 将情绪标签下发 **STM32** 驱动舵机表情（与语音模块独立运行）

---

## 仓库目录

```text
AI-Accompanying-Pet/
├── main.c                          # STM32 固件（RT-Thread）
├── sketch_jun11c_total_trial.ino   # ESP32 固件（Arduino）
├── main.py                         # Mac 语音交互主程序
├── config.py                       # Mac 端全局配置（IP、阈值等）
├── emotion/                        # 情绪识别模块
├── network/                        # Mac ↔ ESP32 UDP 通信
├── speech/                         # 语音识别与合成
├── llm/                            # DeepSeek 对话
└── requirements.txt                # Mac 端 Python 依赖
```

---

## 硬件固件说明

### STM32 — `main.c`（RT-Thread）

| 模块 | 说明 |
|------|------|
| **SU-03T 通信** | UART2（9600），接收离线语音模块十六进制指令（WAKE / STOP / 动作指令等） |
| **舵机控制** | 5 路 PWM 舵机（左前、右前、左后、右后、尾巴），支持前进、后退、转向、坐下、趴下、唤醒摆尾等动作 |
| **ESP32 通信** | UART3（115200），解析 SU-03T 唤醒/停止后向 ESP32 转发 `WAKE\n`、`STOP\n` 等指令 |
| **多线程** | 独立接收线程解析 SU-03T 串口数据，主循环分发命令 |

### ESP32 — `sketch_jun11c_total_trial.ino`

| 模块 | 说明 |
|------|------|
| **屏幕** | ST7789（240×240），状态机驱动 sleep / happy / speak 表情与嘴部动画 |
| **麦克风** | INMP441，I2S 采集 16 kHz PCM |
| **扬声器** | MAX98357A，I2S 播放 Mac 下发的 TTS PCM |
| **WiFi UDP** | 端口 8888，上行发送录音 PCM 至 Mac，下行接收 `TTS_START` / PCM / `TTS_END` |
| **STM32 通信** | UART2，接收 `WAKE` / `STOP`，唤醒时开始录音并通知 Mac |
| **状态机** | `STATE_SLEEP` → `STATE_HAPPY`（唤醒录音）→ `STATE_SPEAK`（播放回复） |

> 烧录前请在固件中配置 **WiFi SSID/密码**、**Mac IP 地址** 及热点网段。

---

## Mac 端软件说明

### 环境要求

- Python 3.10
- 系统工具：`ffmpeg`
- 安装依赖：`pip install -r requirements.txt`

### 配置

编辑 `config.py`：

- `ESP32_IP`、`STM32_IP`、`CAM_CAPTURE_URL`：联调时按热点实际 IP 修改
- `SILENCE_THRESHOLD` 等：按现场环境调整静默检测

DeepSeek API Key 通过环境变量设置（勿写入仓库）：

```bash
export DEEPSEEK_API_KEY="你的密钥"
```

### 运行方式

**两个终端分别启动**（互不阻塞）：

```bash
# 终端 1：情绪识别
python emotion/emotion_detector.py

# 终端 2：语音交互
python main.py
```

### 各模块功能

| 目录/文件 | 功能 |
|-----------|------|
| `main.py` | 调度录音 → STT → LLM → TTS 全流程 |
| `network/audio_receiver.py` | UDP 常驻监听，三态状态机（listening / recording / processing），动态静默检测 |
| `network/audio_sender.py` | `TTS_START` → PCM 分片 → `TTS_END` |
| `speech/stt.py` | faster-whisper 本地语音识别 |
| `llm/deepseek_client.py` | DeepSeek 桌宠式对话生成 |
| `speech/tts.py` | edge-tts 合成 + ffmpeg 转 PCM + 下发 ESP32 |
| `emotion/emotion_detector.py` | ESP32-CAM 取图 → ViT 表情识别 → UDP 下发 STM32 |

### Mac ↔ ESP32 语音协议

| 方向 | 内容 |
|------|------|
| ESP32 → Mac | 唤醒后持续发送 16 kHz / 16 bit / 单声道 PCM |
| Mac 逻辑 | listening 下收到首包 PCM 开录；静默 2 s 保存 WAV；processing 阶段丢弃多余 PCM |
| Mac → ESP32 | `TTS_START`（停录并准备接收）→ PCM 分片（1024 B）→ `TTS_END`（收齐后播放） |

---

## 分工说明

| 部分 | 内容 |
|------|------|
| **Mac 软件** | 情绪识别、语音识别、大模型对话、TTS、UDP 通信协议 |
| **STM32 固件** | RT-Thread 舵机控制、SU-03T 与 ESP32 串口调度 |
| **ESP32 固件** | 屏幕表情、麦克风/喇叭、WiFi UDP 语音链路 |

---

## 仓库分支说明

- **`main`**：完整参赛作品（软件 + 硬件），**提交与打包请以此分支为准**
- **`hardware`**：硬件开发过程分支，内容与 `main` 基本一致，可保留作记录

---

## 许可证

本项目仅用于嵌入式大赛学习与答辩展示。
