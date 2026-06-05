from shield import ShieldDefense
import time

# Imagine this is the OpenAI Python Library
def mock_openai_stream(prompt):
    """Mocks openai.chat.completions.create(..., stream=True)"""
    tokens = ["Hello, ", "this ", "is ", "GPT-4o.", " Let ", "me ", "help ", "you."]
    for t in tokens:
        time.sleep(0.02) # Network latency
        yield t

# 1. Initialize our Defense System
# You can choose 'CTE', 'SJI', or 'ASM'
defense = ShieldDefense(mode='ASM', buffer_rate=0.05)

# 2. Wrap the external service with our defense using .protect()
# This makes it completely Plug-and-Play for any API!
@defense.protect
def secure_openai_query(prompt):
    return mock_openai_stream(prompt)

if __name__ == "__main__":
    print("Streaming from external API through S.H.I.E.L.D...")
    
    # Run the secure query
    generator = secure_openai_query("Help me")
    
    for token in generator:
        print(token, end="", flush=True)
        
    print("\n\nStream complete. The connection was completely side-channel secure.")
