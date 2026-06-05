import os
from dotenv import load_dotenv

load_dotenv()

def generate_response(model_name: str, messages: list, temperature=0.7):
    """
    A unified wrapper for multiple AI providers.
    """
    if model_name.startswith("groq/"):
        from openai import OpenAI
        actual_model = model_name.split("/", 1)[1]
        client = OpenAI(
            api_key=os.environ.get("GROQ_API_KEY"),
            base_url="https://api.groq.com/openai/v1"
        )
        response = client.chat.completions.create(
            model=actual_model,
            messages=messages,
            temperature=temperature
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
            model=actual_model,
            messages=messages,
            temperature=temperature
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
            model=actual_model,
            messages=messages,
            temperature=temperature
        )
        return response.choices[0].message.content

    elif model_name.startswith("gemini"):
        import google.generativeai as genai
        genai.configure(api_key=os.environ.get("GEMINI_API_KEY"))
        
        system_instruction = ""
        for msg in messages:
            if msg["role"] == "system":
                system_instruction += msg["content"] + "\n"
                
        model = genai.GenerativeModel(model_name, system_instruction=system_instruction)
        
        gemini_messages = []
        for msg in messages:
            if msg["role"] == "system": continue
            role = "user" if msg["role"] == "user" else "model"
            gemini_messages.append({"role": role, "parts": [msg["content"]]})
            
        response = model.generate_content(gemini_messages)
        return response.text



def generate_stream(model_name: str, messages: list, temperature=0.7):
    """
    Streaming version of the unified wrapper.
    """
    if model_name.startswith("groq/"):
        from openai import OpenAI
        actual_model = model_name.split("/", 1)[1]
        client = OpenAI(
            api_key=os.environ.get("GROQ_API_KEY"),
            base_url="https://api.groq.com/openai/v1"
        )
        stream = client.chat.completions.create(
            model=actual_model,
            messages=messages,
            temperature=temperature,
            stream=True
        )
        for chunk in stream:
            if chunk.choices[0].delta.content:
                yield chunk.choices[0].delta.content
        
    elif model_name.startswith("gemini"):
        import google.generativeai as genai
        genai.configure(api_key=os.environ.get("GEMINI_API_KEY"))
        
        system_instruction = ""
        for msg in messages:
            if msg["role"] == "system":
                system_instruction += msg["content"] + "\n"
                
        model = genai.GenerativeModel(model_name, system_instruction=system_instruction)
        
        gemini_messages = []
        for msg in messages:
            if msg["role"] == "system": continue
            role = "user" if msg["role"] == "user" else "model"
            gemini_messages.append({"role": role, "parts": [msg["content"]]})
            
        response = model.generate_content(gemini_messages, stream=True)
        for chunk in response:
            yield chunk.text

    else:
        # For demo purposes, we'll fall back to word-by-word simulation for others 
        # if they don't natively support easy streaming in this wrapper yet
        full_text = generate_response(model_name, messages, temperature)
        for word in full_text.split(" "):
            yield word + " "
