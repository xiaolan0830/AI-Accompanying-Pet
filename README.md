# AI Accompanying Pet

智能桌宠 Mac 端软件：情绪识别 + 语音交互（嵌入式大赛提交代码）。

## 功能

- **情绪识别**：ESP32-CAM 取图 → Haar 人脸检测 → ViT 表情分类 → UDP 下发 STM32
- **语音交互**：ESP32 上行 PCM → Whisper 识别 → DeepSeek 对话 → edge-tts 合成 → UDP 下发 ESP32 播放

## 环境

- Python 3.10
- 系统依赖：`ffmpeg`
- 安装：`pip install -r requirements.txt`

## 配置

编辑 `config.py` 中的 IP 地址与静默检测阈值。DeepSeek API Key 通过环境变量设置：

```bash
export DEEPSEEK_API_KEY="你的密钥"
```

## 运行

两个终端分别执行：

```bash
python emotion/emotion_detector.py
python main.py
```

## 目录说明

```
├── main.py                 # 语音交互主程序
├── config.py               # 全局配置
├── emotion/                # 情绪识别
├── network/                # UDP 收发
├── speech/                 # STT / TTS
├── llm/                    # DeepSeek 对话
└── requirements.txt
```

## 通信协议（Mac ↔ ESP32）

- **上行**：ESP32 持续发送 PCM；Mac listening 下首包开录，静默 2s 结束
- **下行**：`TTS_START` → PCM 分片 → `TTS_END`（`TTS_START` 通知 ESP32 停录并准备播放）
