# UDP 常驻监听：listening → recording → processing
# ESP32 唤醒后直接发 PCM；listening 下首包开录；静默 2s 结束；processing 阶段丢弃 PCM

import socket
import threading
import wave
import time

import numpy as np

import config

STATE_LISTENING = "listening"
STATE_RECORDING = "recording"
STATE_PROCESSING = "processing"


def _pcm_volume(data: bytes) -> int:
    samples = np.frombuffer(data, dtype=np.int16)
    if len(samples) == 0:
        return 0
    return int(np.max(np.abs(samples)))


def _save_wav(audio_data: bytearray, wav_file: str) -> None:
    with wave.open(wav_file, "wb") as wf:
        wf.setnchannels(config.CHANNELS)
        wf.setsampwidth(config.SAMPLE_WIDTH)
        wf.setframerate(config.SAMPLE_RATE)
        wf.writeframes(bytes(audio_data))


class AudioReceiver:
    """常驻 UDP 接收器，后台线程维护三态状态机。"""

    def __init__(self):
        self._state = STATE_LISTENING
        self._lock = threading.Lock()
        self._recording_done = threading.Event()
        self._wav_result = None

        self._audio_data = bytearray()
        self._has_speech = False
        self._last_voice_time = 0.0
        self._recording_started_at = 0.0
        self._effective_threshold = config.SILENCE_THRESHOLD
        self._calibration_volumes: list[int] = []
        self._calibration_done = False

        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.bind((config.UDP_IP, config.UDP_PORT))
        self._thread = None

    def start(self) -> None:
        if self._thread is not None:
            return
        self._thread = threading.Thread(target=self._recv_loop, daemon=True)
        self._thread.start()
        print("UDP 监听已启动，等待 ESP32 音频...")

    def wait_for_recording(self) -> str | None:
        """阻塞直到一轮录音结束，返回 WAV 路径；无有效音频时返回 None。"""
        self._recording_done.clear()
        self._recording_done.wait()
        return self._wav_result

    def resume_listening(self) -> None:
        """TTS 下发完成后调用，重新进入 listening。"""
        with self._lock:
            self._state = STATE_LISTENING
            self._reset_recording_state()
        print("继续等待下一轮录音...")

    def _reset_recording_state(self) -> None:
        self._audio_data.clear()
        self._has_speech = False
        self._calibration_volumes.clear()
        self._calibration_done = False
        self._effective_threshold = config.SILENCE_THRESHOLD

    def _begin_recording(self, data: bytes) -> None:
        self._state = STATE_RECORDING
        self._reset_recording_state()
        self._recording_started_at = time.time()
        self._last_voice_time = self._recording_started_at
        self._audio_data.extend(data)
        self._update_volume(data, is_first_packet=True)
        print("检测到新一轮 PCM，开始录音")

    def _calibrate_threshold(self) -> None:
        if not self._calibration_volumes:
            return
        noise_floor = float(np.median(self._calibration_volumes))
        dynamic = noise_floor * config.NOISE_THRESHOLD_MULTIPLIER
        self._effective_threshold = max(
            config.MIN_SILENCE_THRESHOLD,
            dynamic,
            config.SILENCE_THRESHOLD * 0.5,
        )
        self._calibration_done = True
        print(f"噪声底={noise_floor:.0f}，动态阈值={self._effective_threshold:.0f}")

    def _update_volume(self, data: bytes, is_first_packet: bool = False) -> None:
        try:
            volume = _pcm_volume(data)
        except Exception as e:
            print("音频解析错误:", e)
            return

        print("volume =", volume)

        now = time.time()
        if not self._calibration_done:
            self._calibration_volumes.append(volume)
            elapsed = now - self._recording_started_at
            if elapsed >= config.NOISE_CALIBRATION_SECONDS:
                self._calibrate_threshold()

        threshold = (
            self._effective_threshold
            if self._calibration_done
            else config.SILENCE_THRESHOLD
        )

        if volume > threshold:
            if not self._has_speech:
                print("检测到语音")
            self._has_speech = True
            self._last_voice_time = now

    def _finish_recording(self) -> None:
        print("检测到静默，录音结束")
        wav_file = config.RECEIVED_WAV_FILE

        if len(self._audio_data) == 0:
            print("未收到有效音频")
            self._state = STATE_LISTENING
            self._reset_recording_state()
            self._wav_result = None
        else:
            _save_wav(self._audio_data, wav_file)
            print(f"保存完成：{wav_file}")
            print(f"音频大小：{len(self._audio_data)} 字节")
            self._state = STATE_PROCESSING
            self._wav_result = wav_file

        self._recording_done.set()

    def _abort_recording(self, reason: str) -> None:
        print(reason)
        self._state = STATE_LISTENING
        self._reset_recording_state()
        self._wav_result = None
        self._recording_done.set()

    def _handle_recording_packet(self, data: bytes) -> None:
        self._audio_data.extend(data)
        self._update_volume(data)

        now = time.time()
        elapsed = now - self._recording_started_at

        if elapsed > config.MAX_RECORDING_SECONDS:
            if self._has_speech:
                self._finish_recording()
            else:
                self._abort_recording("录音超时且未检测到语音，丢弃本轮")
            return

        if (
            self._calibration_done
            and not self._has_speech
            and elapsed > config.NO_SPEECH_TIMEOUT
        ):
            self._abort_recording("长时间未检测到语音，丢弃本轮")
            return

        if not self._has_speech:
            return

        silence_time = now - self._last_voice_time
        if silence_time > config.SILENCE_DURATION:
            self._finish_recording()

    def _recv_loop(self) -> None:
        while True:
            data, _addr = self._sock.recvfrom(4096)

            with self._lock:
                if self._state == STATE_PROCESSING:
                    continue

                if self._state == STATE_LISTENING:
                    self._begin_recording(data)
                    continue

                if self._state == STATE_RECORDING:
                    self._handle_recording_packet(data)


def receive_audio() -> str | None:
    """兼容旧接口：单轮阻塞接收（不推荐，主程序请用 AudioReceiver）。"""
    receiver = AudioReceiver()
    receiver.start()
    result = receiver.wait_for_recording()
    receiver._sock.close()
    return result
