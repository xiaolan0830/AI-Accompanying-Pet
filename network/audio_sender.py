# 读取 PCM → 发送 TTS_START → 分块 PCM → TTS_END

import socket
import time

import config


def send_pcm_file(pcm_file: str) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    dest = (config.ESP32_IP, config.ESP32_PORT)
    total_bytes = 0

    print("发送 TTS_START...")
    sock.sendto(b"TTS_START", dest)

    with open(pcm_file, "rb") as f:
        while True:
            chunk = f.read(config.CHUNK_SIZE)
            if not chunk:
                break
            sock.sendto(chunk, dest)
            total_bytes += len(chunk)
            time.sleep(config.PCM_SEND_DELAY)

    print("发送 TTS_END...")
    sock.sendto(b"TTS_END", dest)
    sock.close()

    print(f"PCM 发送完成，共发送 {total_bytes} 字节")
