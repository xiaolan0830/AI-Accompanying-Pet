import asyncio
import os
import re
import subprocess
from contextlib import contextmanager

import edge_tts

import config
from network.audio_sender import send_pcm_file

_PROXY_ENV_KEYS = (
    "http_proxy", "https_proxy", "all_proxy",
    "HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY",
)


@contextmanager
def _without_proxy_env():
    saved = {k: os.environ.pop(k) for k in _PROXY_ENV_KEYS if k in os.environ}
    try:
        yield
    finally:
        os.environ.update(saved)


def strip_stage_directions(text: str) -> str:
    """去掉括号内的动作/情绪描写，避免被 TTS 朗读。"""
    text = re.sub(r"[（(][^）)]*[）)]", "", text)
    text = re.sub(r"\s+", " ", text).strip()
    return text


async def generate_tts(text: str) -> None:
    with _without_proxy_env():
        communicate = edge_tts.Communicate(text, voice=config.TTS_VOICE)
        await communicate.save("reply.mp3")


def mp3_to_pcm() -> None:
    subprocess.run([
        "ffmpeg", "-y",
        "-i", "reply.mp3",
        "-f", "s16le",
        "-acodec", "pcm_s16le",
        "-ac", "1",
        "-ar", str(config.SAMPLE_RATE),
        "reply.pcm",
    ], check=True)


def send_tts_to_esp32(text: str) -> None:
    text = strip_stage_directions(text)
    if not text:
        print("TTS 文本为空，跳过播放")
        return

    print("生成 TTS...")
    asyncio.run(generate_tts(text))

    print("转换 PCM...")
    mp3_to_pcm()

    print("发送 PCM 到 ESP32...")
    send_pcm_file("reply.pcm")
    print("发送完成")
