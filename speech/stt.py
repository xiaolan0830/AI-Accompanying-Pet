# 加载Whisper模型(small/int8) → 转写WAV文件 → 拼接文本片段 → 返回文字

from faster_whisper import WhisperModel

# 模型初始化
model = WhisperModel("small", compute_type="int8")

def speech_to_text(wav_file):
    """
    将wav音频转换成文字
    """
    segments, _ = model.transcribe(wav_file)
    text = ""
    for segment in segments:
        text += segment.text
    return text