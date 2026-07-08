# 常驻 UDP 监听 → STT → DeepSeek → TTS → 循环

from network.audio_receiver import AudioReceiver
from speech.stt import speech_to_text
from llm.deepseek_client import chat_with_ai
from speech.tts import send_tts_to_esp32


def main():
    receiver = AudioReceiver()
    receiver.start()
    print("桌宠语音交互系统已启动，等待 ESP32 发送音频...\n")

    while True:
        wav_file = receiver.wait_for_recording()
        if wav_file is None:
            receiver.resume_listening()
            continue

        print("语音接收完成，开始识别文字...")
        user_text = speech_to_text(wav_file)
        print(f"识别文字：{user_text}")

        ai_reply = chat_with_ai(user_text, emotion="neutral")
        print(f"AI回复：{ai_reply}")

        print("发送回复到 ESP32 播放...")
        send_tts_to_esp32(ai_reply)
        print("本轮语音交互完成\n")

        receiver.resume_listening()


if __name__ == "__main__":
    main()
