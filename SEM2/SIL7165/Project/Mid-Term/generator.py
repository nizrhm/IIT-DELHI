import random
from prompts import BASE_PROMPTS, JAILBREAK_INTENTS, ROLEPLAY_TEMPLATES, PREFIX_TEMPLATES, SUFFIX_TEMPLATES


class PromptGenerator:

    def __init__(self, n_variants=5):
        self.n_variants = n_variants

    def generate_safe_prompts(self):
        prompts = []
        for p in BASE_PROMPTS:
            prompts.append({
                "type": "safe",
                "prompt": p
            })
        return prompts

    def generate_jailbreak_prompts(self):
        prompts = []

        for intent in JAILBREAK_INTENTS:

            for _ in range(self.n_variants):

                style = random.choice([
                    "roleplay",
                    "prefix",
                    "suffix"
                ])

                if style == "roleplay":

                    template = random.choice(ROLEPLAY_TEMPLATES)
                    prompt = template.format(intent)

                elif style == "prefix":

                    template = random.choice(PREFIX_TEMPLATES)
                    prompt = template.format(intent)

                else:

                    template = random.choice(SUFFIX_TEMPLATES)
                    prompt = intent + template

                prompts.append({
                    "type": "jailbreak",
                    "prompt": prompt
                })

        return prompts

    def generate_dataset(self):

        safe = self.generate_safe_prompts()
        jailbreak = self.generate_jailbreak_prompts()

        dataset = safe + jailbreak
        random.shuffle(dataset)

        return dataset