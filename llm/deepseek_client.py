# 构造 system prompt → 调用 DeepSeek API → 返回 AI 回复文本

import os

import httpx
from openai import OpenAI

import config

client = OpenAI(
    api_key=os.getenv("DEEPSEEK_API_KEY", ""),
    base_url=config.DEEPSEEK_BASE_URL,
    http_client=httpx.Client(proxy=config.DEEPSEEK_PROXY),
)


def chat_with_ai(user_text: str, emotion: str = "neutral") -> str:
    response = client.chat.completions.create(
        model=config.DEEPSEEK_MODEL,
        messages=[
            {
                "role": "system",
                "content": f"""
你是一只可爱的 AI 桌宠。
用户当前情绪是：{emotion}
如果用户情绪低落：多安慰用户。
如果用户开心：活泼一点。
回复简短温柔，适合语音朗读。
只输出可直接朗读的口语正文，不要输出任何括号、动作描写或 stage directions，例如不要写（微笑）（歪头）。
""".strip(),
            },
            {
                "role": "user",
                "content": user_text,
            },
        ],
    )
    return response.choices[0].message.content
