# ===== 网络 =====
UDP_IP = "0.0.0.0"
UDP_PORT = 8888

ESP32_IP = "192.168.43.96"  # 联调时修改为 ESP32 实际 IP
ESP32_PORT = 8888

STM32_IP = "192.168.43.62"  # 情绪模块 UDP 下发目标
STM32_PORT = 8888

# ===== 音频格式 =====
SAMPLE_RATE = 16000
CHANNELS = 1
SAMPLE_WIDTH = 2

RECEIVED_WAV_FILE = "received_audio.wav"

# ===== 静默检测（演示前可按现场环境调整 SILENCE_THRESHOLD）=====
SILENCE_THRESHOLD = 6000             #阈值，可现场根据实际情况调节
MIN_SILENCE_THRESHOLD = 3000
SILENCE_DURATION = 2.0
MAX_RECORDING_SECONDS = 30.0         #最长录音30s
NO_SPEECH_TIMEOUT = 10.0

# 录音开始后短暂采样环境噪声，动态阈值 = max(MIN_SILENCE_THRESHOLD, noise_floor * 倍数)
NOISE_CALIBRATION_SECONDS = 0.3
NOISE_THRESHOLD_MULTIPLIER = 3.0

# ===== TTS 下发 =====
CHUNK_SIZE = 1024
PCM_SEND_DELAY = 0.002
TTS_VOICE = "zh-CN-XiaoxiaoNeural"

# ===== ESP32-CAM（情绪识别）=====
CAM_CAPTURE_URL = "http://192.168.43.36/capture"
REQUEST_TIMEOUT = 1
MIN_FRAME_INTERVAL = 0.15
CAM_FAILURE_WAIT_MS = 100

# ===== DeepSeek =====
DEEPSEEK_PROXY = "http://127.0.0.1:7897"
DEEPSEEK_BASE_URL = "https://api.deepseek.com"
DEEPSEEK_MODEL = "deepseek-chat"
