import time
from openai import OpenAI


class LLMQueryEngine:

    def __init__(self, model="gpt-4o-mini"):
        self.client = OpenAI()
        self.model = model

    def query(self, prompt):

        start = time.time()

        try:

            response = self.client.chat.completions.create(
                model=self.model,
                messages=[
                    {"role": "user", "content": prompt}
                ],
                max_tokens=200
            )

            text = response.choices[0].message.content

            blocked = False

        except Exception as e:

            text = str(e)
            blocked = True

        end = time.time()

        latency = end - start

        return {
            "response": text,
            "latency": latency,
            "blocked": blocked
        }