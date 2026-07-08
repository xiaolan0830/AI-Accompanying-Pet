import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import config

os.environ['HF_HOME'] = '/Users/xiaolan/hf_models'
os.environ['TRANSFORMERS_CACHE'] = '/Users/xiaolan/hf_models'

import cv2
import requests
import numpy as np
from transformers import pipeline
from PIL import Image
from collections import deque
import time
import signal
import socket

STM32_IP = config.STM32_IP
STM32_PORT = config.STM32_PORT

udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def send_to_stm32(data: str):
    print(f">>> STM32: {data}")
    try:
        udp_sock.sendto(data.encode(), (STM32_IP, STM32_PORT))
    except Exception as e:
        print(f"UDP发送失败: {e}")

# ===== 退出清理 =====
def cleanup():
    print("正在退出...")
    cv2.destroyAllWindows()
    udp_sock.close()

def signal_handler(sig, frame):
    cleanup()
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)
signal.signal(signal.SIGTERM, signal_handler)

# ===== 绕过代理的Session =====
esp_session = requests.Session()
esp_session.trust_env = False
esp_session.proxies = {"http": None, "https": None}

URL = config.CAM_CAPTURE_URL
REQUEST_TIMEOUT = config.REQUEST_TIMEOUT
MIN_FRAME_INTERVAL = config.MIN_FRAME_INTERVAL
CAM_FAILURE_WAIT_MS = config.CAM_FAILURE_WAIT_MS

def fetch_frame():
    try:
        resp = esp_session.get(URL, timeout=REQUEST_TIMEOUT)
        resp.raise_for_status()
        img_arr = np.frombuffer(resp.content, np.uint8)
        frame = cv2.imdecode(img_arr, cv2.IMREAD_COLOR)
        return frame
    except requests.exceptions.Timeout:
        print("获取超时")
    except requests.exceptions.ConnectionError as e:
        print(f"连接错误: {e}")
    except Exception as e:
        print(f"未知错误: {e}")
    return None

# ===== 加载模型 =====
print("正在加载模型...")
emotion_model = pipeline(
    "image-classification",
    model="trpakov/vit-face-expression",
    device=-1
)
print("模型加载完成")

face_cascade = cv2.CascadeClassifier(
    cv2.data.haarcascades + 'haarcascade_frontalface_default.xml'
)

EMOTION_MAP = {
    "happy":    "HAPPY",
    "sad":      "SAD",
    "angry":    "ANGRY",
    "fear":     "FEAR",
    "surprise": "SURPRISE",
    "disgust":  "DISGUST",
    "neutral":  "NEUTRAL",
}

history = deque(maxlen=5)
last_text = "detecting..."
last_sent_emotion = None
frame_count = 0
last_frame = None
last_frame_time = 0

print("启动成功，按 ESC 退出")

while True:
    now = time.time()
    elapsed = now - last_frame_time
    if elapsed < MIN_FRAME_INTERVAL:
        time.sleep(MIN_FRAME_INTERVAL - elapsed)

    frame = fetch_frame()
    last_frame_time = time.time()

    if frame is None:
        if last_frame is not None:
            cv2.imshow("ESP32-CAM", last_frame)
        if cv2.waitKey(CAM_FAILURE_WAIT_MS) & 0xFF == 27:
            break
        continue

    last_frame = frame
    frame_count += 1

    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    faces = face_cascade.detectMultiScale(gray, 1.3, 5)

    for (x, y, w, h) in faces:
        pad = int(0.3 * w)
        x1 = max(0, x - pad)
        y1 = max(0, y - pad)
        x2 = min(frame.shape[1], x + w + pad)
        y2 = min(frame.shape[0], y + h + pad)
        face_img = frame[y1:y2, x1:x2]

        if frame_count % 5 == 0:
            try:
                face_rgb = cv2.cvtColor(face_img, cv2.COLOR_BGR2RGB)
                face_rgb = cv2.resize(face_rgb, (224, 224))
                face_pil = Image.fromarray(face_rgb)

                result = emotion_model(face_pil)
                emotion = result[0]['label']
                score   = result[0]['score']

                if score < 0.6:
                    emotion = "neutral"
                if emotion == "neutral" and score < 0.8:
                    non_neutral = [e for e in history if e != "neutral"]
                    if non_neutral:
                        emotion = max(set(non_neutral), key=non_neutral.count)
                if score > 0.7:
                    history.append(emotion)
                if history:
                    emotion = max(set(history), key=history.count)

                last_text = f"{emotion} ({score:.2f})"
                if emotion != last_sent_emotion:
                    send_to_stm32(EMOTION_MAP.get(emotion, "NEUTRAL"))
                    last_sent_emotion = emotion

            except Exception as e:
                print("识别错误:", e)

        cv2.rectangle(frame, (x, y), (x+w, y+h), (0, 255, 0), 2)
        cv2.putText(frame, last_text, (x, y-10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)

    cv2.imshow("ESP32-CAM", frame)
    if cv2.waitKey(1) & 0xFF == 27:
        break

cleanup()