"""
Multi-Provider LLM API Wrapper
================================
Unified interface for Groq, DeepSeek, OpenRouter, and Gemini.
Supports both synchronous (generate_response) and streaming (generate_stream) modes.
"""

import os
from dotenv import load_dotenv

load_dotenv()


def generate_response(model_name: str, messages: list, temperature=0.7):
    """Synchronous completion - returns the full response text."""

    if model_name.startswith("groq/"):
        from openai import OpenAI
        actual_model = model_name.split("/", 1)[1]
        client = OpenAI(
            api_key=os.environ.get("GROQ_API_KEY"),
            base_url="https://api.groq.com/openai/v1"
        )
        response = client.chat.completions.create(
            model=actual_model, messages=messages, temperature=temperature
        )
        return response.choices[0].message.content

    elif model_name.startswith("deepseek/"):
        from openai import OpenAI
        actual_model = model_name.split("/", 1)[1]
        client = OpenAI(
            api_key=os.environ.get("DEEPSEEK_API_KEY"),
            base_url="https://api.deepseek.com"
        )
        response = client.chat.completions.create(
            model=actual_model, messages=messages, temperature=temperature
        )
        return response.choices[0].message.content

    elif model_name.startswith("openrouter/"):
        from openai import OpenAI
        actual_model = model_name.split("/", 1)[1]
        client = OpenAI(
            api_key=os.environ.get("OPEN_ROUTER_KEY"),
            base_url="https://openrouter.ai/api/v1"
        )
        response = client.chat.completions.create(
            model=actual_model, messages=messages, temperature=temperature
        )
        return response.choices[0].message.content

    elif model_name.startswith("gemini"):
        import google.generativeai as genai
        genai.configure(api_key=os.environ.get("GEMINI_API_KEY"))
        system_instruction = "\n".join(
            m["content"] for m in messages if m["role"] == "system"
        )
        model = genai.GenerativeModel(model_name, system_instruction=system_instruction)
        gemini_msgs = [
            {"role": "user" if m["role"] == "user" else "model", "parts": [m["content"]]}
            for m in messages if m["role"] != "system"
        ]
        response = model.generate_content(gemini_msgs)
        return response.text

    else:
        raise ValueError(f"Unsupported model: {model_name}")


def generate_stream(model_name: str, messages: list, temperature=0.7):
    """Streaming completion - yields tokens one at a time."""

    if model_name.startswith("groq/"):
        from openai import OpenAI
        actual_model = model_name.split("/", 1)[1]
        client = OpenAI(
            api_key=os.environ.get("GROQ_API_KEY"),
            base_url="https://api.groq.com/openai/v1"
        )
        stream = client.chat.completions.create(
            model=actual_model, messages=messages,
            temperature=temperature, stream=True
        )
        for chunk in stream:
            if chunk.choices[0].delta.content:
                yield chunk.choices[0].delta.content

    elif model_name.startswith("gemini"):
        import google.generativeai as genai
        genai.configure(api_key=os.environ.get("GEMINI_API_KEY"))
        system_instruction = "\n".join(
            m["content"] for m in messages if m["role"] == "system"
        )
        model = genai.GenerativeModel(model_name, system_instruction=system_instruction)
        gemini_msgs = [
            {"role": "user" if m["role"] == "user" else "model", "parts": [m["content"]]}
            for m in messages if m["role"] != "system"
        ]
        response = model.generate_content(gemini_msgs, stream=True)
        for chunk in response:
            yield chunk.text

    else:
        # Fallback: simulate streaming from synchronous response
        full_text = generate_response(model_name, messages, temperature)
        for word in full_text.split(" "):
            yield word + " "
