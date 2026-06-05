"""
Attack Module 2: Encrypted Packet Eavesdropping (Interactive)
==============================================================
Interactive chat console where you type messages. Behind the scenes:
  - Each message is tokenized and "encrypted" over TLS
  - An eavesdropper intercepts packet sizes
  - A Viterbi decoder tries to reconstruct your plaintext

Run with shield OFF to see the attack succeed.
Run with shield ON to see Constant-Size Padding defeat it.

WHERE THE VULNERABILITY IS:
  SecureNetwork.transmit_token() — packet_size = len(token) + TLS_overhead

WHERE THE DEFENSE IS:
  SecureNetwork.transmit_token() with protected=True — pads to uniform size
"""

import time
import math
import random
from collections import defaultdict


# ── NETWORK SIMULATION ────────────────────────────────────────────

class SecureNetwork:
    """Simulates TLS-encrypted network transport."""
    TLS_OVERHEAD_BYTES = 29

    def __init__(self, protected=False):
        self.packet_log = []
        self.protected = protected

    def transmit_token(self, token: str):
        real_length = len(token)
        if self.protected:
            padded_length = random.randint(14, 16)
        else:
            padded_length = real_length

        pkt_size = padded_length + self.TLS_OVERHEAD_BYTES
        entry = {
            "token": token,
            "real_length": real_length,
            "transmitted_length": padded_length,
            "packet_size": pkt_size,
        }
        self.packet_log.append(entry)
        return entry

    def get_leaked_lengths(self):
        return [p["transmitted_length"] for p in self.packet_log]

    def clear(self):
        self.packet_log = []


# ── ATTACKER: VITERBI DECODER ─────────────────────────────────────

class AITokenReconstructor:
    """Bigram language model + Viterbi beam search."""

    def __init__(self):
        self.unigrams = defaultdict(int)
        self.bigrams = defaultdict(int)
        self.total_words = 0
        self.length_to_words = defaultdict(set)

    def train(self, corpus: list):
        for sentence in corpus:
            words = [w + " " for w in sentence.split(" ")]
            words[-1] = words[-1].strip()
            for i, word in enumerate(words):
                self.unigrams[word] += 1
                self.total_words += 1
                self.length_to_words[len(word)].add(word)
                if i < len(words) - 1:
                    self.bigrams[(word, words[i + 1])] += 1

    def _score_bigram(self, w1, w2):
        c1 = self.unigrams.get(w1, 0)
        cb = self.bigrams.get((w1, w2), 0)
        return math.log((cb + 1) / (c1 + len(self.unigrams)))

    def infer(self, lengths, top_n=3):
        candidates = []
        for L in lengths:
            words = list(self.length_to_words.get(L, []))
            if not words:
                return ["<NO MATCH: uniform padding destroyed signal>"]
            candidates.append(words)

        paths = [
            (math.log((self.unigrams[w] + 1) / (self.total_words + len(self.unigrams))), [w])
            for w in candidates[0]
        ]
        for i in range(1, len(lengths)):
            new_paths = []
            for nw in candidates[i]:
                for score, prev in paths:
                    t = self._score_bigram(prev[-1], nw)
                    new_paths.append((score + t, prev + [nw]))
            new_paths.sort(key=lambda x: x[0], reverse=True)
            paths = new_paths[:100]

        paths.sort(key=lambda x: x[0], reverse=True)
        return ["".join(p) for _, p in paths[:top_n]]


# ── TRAINING DATA (Vastly Expanded for Robustness) ────────────────

CORPUS = [
    "Project Titan is delayed",
    "Project Orion is launching tomorrow",
    "The password is admin",
    "The nuclear launch code",
    "Send money to this account",
    "Company earnings dropped by ten percent",
    "Meeting canceled until further notice",
    "fire the manager immediately",
    "the server crashed again",
    "deploy to production now",
    "cancel the merger deal",
    "hire new engineers today",
    "hello how are you doing today",
    "is it going well for you",
    "i am doing fine thank you",
    "what is the status of the project",
    "please provide the server ip address",
    "the database is down for maintenance",
    "we need to fix the security bug",
    "access is denied for this user",
    "the system is running smoothly",
    "can you help me with this task",
    "this is a top secret message",
    "do not share this with anyone",
    "the meeting is scheduled for tomorrow",
    "we are launching a new product",
    "the marketing campaign is ready",
    "the sales team is performing well",
    "the revenue increased by twenty percent",
    "we need to reduce our costs",
    "the budget is approved for the year",
    "the financial report is attached",
    "please review the document",
    "the feedback is very helpful",
    "we will implement the changes",
    "the project is on track",
    "the deadline is approaching soon",
    "we need to work faster",
    "the quality is very high",
    "the customers are satisfied",
    "the market share is growing",
    "the competition is very strong",
    "we need to innovate more",
    "the technology is advanced",
    "the software is stable",
    "the hardware is reliable",
    "the network is secure",
    "the data is encrypted",
    "the privacy is protected",
    "the security is enhanced",
    "the framework is robust",
    "the performance is optimized",
    "the scalability is high",
    "the availability is constant",
    "the reliability is guaranteed",
    "the efficiency is improved",
    "the productivity is increased",
    "the cost is reduced",
    "the time is saved",
    "the effort is minimized",
    "the results are positive",
    "the outcome is successful",
    "the goal is achieved",
    "the mission is complete",
    "the vision is clear",
    "the strategy is effective",
    "the execution is flawless",
    "the leadership is strong",
    "the teamwork is great",
    "the culture is positive",
    "the values are aligned",
    "the mission is important",
    "the future is bright",
    "the growth is sustainable",
    "the impact is significant",
    "the value is high",
    "the quality is top notch",
    "the service is excellent",
    "the support is great",
    "the experience is positive",
    "the satisfaction is high",
    "the loyalty is strong",
    "the retention is high",
    "the acquisition is fast",
    "the conversion is high",
    "the engagement is strong",
    "the reach is wide",
    "the brand is strong",
    "the reputation is good",
    "the trust is high",
    "the authority is strong",
    "the influence is wide",
    "the impact is global",
    "the reach is worldwide",
    "the scale is massive",
    "the speed is fast",
    "the agility is high",
    "the flexibility is great",
    "the adaptability is strong",
    "the resilience is high",
    "the endurance is long",
    "the strength is massive",
    "the power is great",
    "the influence is strong",
    "the impact is positive",
    "the value is immense",
    "the potential is huge",
    "the opportunity is great",
    "the challenge is real",
    "the solution is effective",
    "the method is proven",
    "the technique is advanced",
    "the process is efficient",
    "the workflow is smooth",
    "the system is optimized",
    "the platform is robust",
    "the architecture is scalable",
    "the design is elegant",
    "the code is clean",
    "the logic is sound",
    "the data is accurate",
    "the information is useful",
    "the knowledge is valuable",
    "the insight is deep",
    "the wisdom is profound",
    "the understanding is clear",
    "the clarity is high",
    "the focus is sharp",
    "the attention is detail",
    "the care is high",
    "the passion is strong",
    "the dedication is great",
    "the commitment is high",
    "the integrity is strong",
    "the honesty is high",
    "the transparency is clear",
    "the accountability is high",
    "the responsibility is great",
    "the respect is strong",
    "the empathy is high",
    "the compassion is great",
    "the kindness is strong",
    "the love is high",
    "the peace is deep",
    "the joy is great",
    "the happiness is high",
    "the fulfillment is strong",
    "the satisfaction is deep",
    "the meaning is profound",
    "the purpose is clear",
    "the destiny is bright",
    "the journey is long",
    "the path is clear",
    "the steps are small",
    "the progress is steady",
    "the momentum is strong",
    "the velocity is high",
    "the acceleration is fast",
    "the speed is constant",
    "the time is now",
    "the place is here",
    "the world is large",
    "the universe is vast",
    "the mystery is deep",
    "the truth is simple",
    "the answer is within",
    "the solution is simple",
    "the problem is solved",
    "the task is complete",
    "the work is done",
    "the end is near",
    "the beginning is new",
    "the cycle is complete",
    "the circle is round",
    "the square is flat",
    "the triangle is sharp",
    "the line is straight",
    "the curve is smooth",
    "the shape is unique",
    "the color is bright",
    "the sound is clear",
    "the light is shining",
    "the dark is fading",
    "the day is sunny",
    "the night is quiet",
    "the wind is blowing",
    "the rain is falling",
    "the snow is cold",
    "the fire is hot",
    "the water is cool",
    "the earth is solid",
    "the air is fresh",
    "the life is good",
    "the soul is free",
    "the mind is calm",
    "the body is strong",
    "the spirit is high",
    "the energy is positive",
    "the vibes are great",
    "the mood is happy",
    "the feeling is right",
    "the timing is perfect",
    "the moment is now",
    "the here is now",
    "the today is yours",
    "the tomorrow is bright",
    "the always is forever",
    "the never is ending",
    "the everything is possible",
    "the nothing is impossible",
    "the one is enough",
    "the many are plenty",
    "the all are welcome",
    "the world is one",
    "the unity is strength",
    "the peace is power",
    "the love is light",
    "the truth is freedom",
    "the wisdom is wealth",
    "the health is happiness",
    "the family is first",
    "the friends are forever",
    "the community is key",
    "the society is strong",
    "the nation is great",
    "the world is beautiful",
    "the nature is divine",
    "the forest is green",
    "the mountain is high",
    "the ocean is deep",
    "the river is long",
    "the lake is calm",
    "the sky is blue",
    "the sun is golden",
    "the moon is silver",
    "the stars are bright",
    "the cloud is white",
    "the storm is loud",
    "the peace is quiet",
    "the morning is fresh",
    "the evening is soft",
    "the spring is new",
    "the summer is warm",
    "the autumn is gold",
    "the winter is white",
    "the garden is full",
    "the flower is red",
    "the tree is tall",
    "the bird is flying",
    "the fish is swimming",
    "the horse is running",
    "the lion is roaring",
    "the eagle is soaring",
    "the wolf is howling",
    "the cat is purring",
    "the dog is barking",
    "the human is thinking",
    "the person is kind",
    "the child is playing",
    "the student is learning",
    "the teacher is helping",
    "the doctor is healing",
    "the builder is making",
    "the artist is creating",
    "the writer is telling",
    "the musician is playing",
    "the dancer is moving",
    "the runner is winning",
    "the swimmer is fast",
    "the gamer is playing",
    "the coder is coding",
    "the hacker is hacking",
    "the user is happy",
    "the customer is king",
    "the client is priority",
    "the project is success",
    "the task is finished",
    "the goal is met",
    "the dream is real",
    "the reality is good",
    "the hope is strong",
    "the faith is deep",
    "the trust is solid",
    "the belief is powerful",
    "the magic is real",
    "the wonder is great",
    "the curiosity is high",
    "the adventure is starting",
    "the quest is ongoing",
    "the hero is you",
    "the legend is now",
    "the history is making",
    "the future is writing",
    "the now is yours",
    "the end is start",
    "we must proceed with caution",
    "authentication bypass vulnerability found",
    "zero day exploit detected in the wild",
    "patch the server immediately to prevent breach",
    "the firewall is blocking the malicious traffic",
    "data exfiltration attempt blocked by shield",
    "secure your api keys with environment variables",
    "do not commit secrets to version control",
    "enable multi factor authentication for all users",
    "rotate your passwords every ninety days",
    "use a strong encryption algorithm for storage",
    "sanitize all user inputs to prevent injection",
    "implement rate limiting to mitigate denial of service",
    "monitor your logs for suspicious activity",
    "conduct regular security audits and pentests",
    "train your employees on phishing awareness",
    "keep your software and dependencies up to date",
    "follow the principle of least privilege",
    "segment your network to contain potential breaches",
    "backup your data regularly to a secure location",
    "incident response plan is ready for execution",
    "security is a process not a product",
    "protect your digital assets with high assurance",
    "the threat landscape is constantly evolving",
    "stay vigilant and proactive in your defense",
    "cyber security is everyone's responsibility",
    "defense in depth is the best strategy",
    "minimize the attack surface area",
    "encrypt data at rest and in transit",
    "secure coding practices are essential",
    "perform vulnerability scanning on every build",
    "the security framework is successfully deployed",
    "all side channels are effectively masked",
    "adversarial attacks are neutralized by shield",
    "the privacy of our users is our top priority",
    "we are committed to excellence in security",
    "the project was a great success overall",
    "thank you for your hard work and dedication",
    "we look forward to the next challenge together",
    "the future of ai is secure with shield",
    "stay safe and secure in the digital world",
]


# ── INTERACTIVE MODE ──────────────────────────────────────────────

def run_interactive(shield_on):
    """Interactive chat mode where user types messages and sees interception."""
    mode = "S.H.I.E.L.D. ON" if shield_on else "UNPROTECTED"
    network = SecureNetwork(protected=shield_on)
    decoder = AITokenReconstructor()
    decoder.train(CORPUS)

    print(f"\n  {'='*60}")
    print(f"  PACKET EAVESDROPPING CONSOLE ({mode})")
    print(f"  {'='*60}")
    print(f"  Type messages below. Each one is 'encrypted' and sent over TLS.")
    print(f"  An attacker is sniffing the wire and trying to decode them.")
    print(f"  Type 'quit' to exit.\n")

    msg_num = 0
    while True:
        try:
            user_msg = input("  [YOU] > ").strip()
        except (EOFError, KeyboardInterrupt):
            break
        if not user_msg or user_msg.lower() == "quit":
            break

        msg_num += 1
        network.clear()

        # Tokenize and transmit
        tokens = [w + " " for w in user_msg.split(" ")]
        tokens[-1] = tokens[-1].strip()

        print(f"\n  --- Message #{msg_num}: Transmission Log ---")
        print(f"  {'Token':<15} | {'Real Len':>8} | {'Wire Len':>8} | {'Pkt Size':>8}")
        print(f"  {'-'*15}-+-{'-'*8}-+-{'-'*8}-+-{'-'*8}")

        for token in tokens:
            entry = network.transmit_token(token)
            real = entry["real_length"]
            wire = entry["transmitted_length"]
            pkt = entry["packet_size"]
            match = "" if real == wire else " (PADDED)"
            print(f"  {repr(token):<15} | {real:>8} | {wire:>8} | {pkt:>8}{match}")

        # Attacker intercepts
        leaked = network.get_leaked_lengths()
        print(f"\n  [ATTACKER] Intercepted lengths: {leaked}")

        # Viterbi decode
        predictions = decoder.infer(leaked, top_n=3)
        print(f"  [ATTACKER] Viterbi top-3 guesses:")
        recovered = False
        for i, pred in enumerate(predictions):
            match = " <<< RECOVERED!" if pred == user_msg else ""
            if pred == user_msg:
                recovered = True
            print(f"    {i+1}. \"{pred}\"{match}")

        if recovered:
            print(f"  [!] PRIVACY BREACH: Your exact message was recovered!\n")
        elif shield_on:
            print(f"  [-] DEFENDED: Padding blocked the attack.\n")
        else:
            print(f"  [-] Not in corpus, but length pattern still leaked.\n")


# ── BATCH MODE (for automated testing) ────────────────────────────

def run(shield_on=False):
    """Batch mode — runs a preset demo for logging purposes."""
    print("=" * 70)
    print(f"  PACKET EAVESDROPPING {'(S.H.I.E.L.D. ON)' if shield_on else '(UNPROTECTED)'}")
    print("=" * 70)

    decoder = AITokenReconstructor()
    decoder.train(CORPUS)
    network = SecureNetwork(protected=shield_on)

    victim_secret = "Project Titan is delayed"
    tokens = [w + " " for w in victim_secret.split(" ")]
    tokens[-1] = tokens[-1].strip()

    print(f"\n  [STEP 1] Victim sends: \"{victim_secret}\"")
    print(f"  {'Token':<15} | {'Real Len':>8} | {'Wire Len':>8}")
    print(f"  {'-'*15}-+-{'-'*8}-+-{'-'*8}")
    for token in tokens:
        entry = network.transmit_token(token)
        r, w = entry["real_length"], entry["transmitted_length"]
        pad = " (PADDED)" if r != w else ""
        print(f"  {repr(token):<15} | {r:>8} | {w:>8}{pad}")

    leaked = network.get_leaked_lengths()
    print(f"\n  [STEP 2] Attacker sniffs packet sizes: {leaked}")

    predictions = decoder.infer(leaked, top_n=3)
    print(f"  [STEP 3] Viterbi decoder output:")
    success = False
    for i, pred in enumerate(predictions):
        m = " <<< MATCH!" if pred == victim_secret else ""
        if pred == victim_secret:
            success = True
        print(f"    {i+1}. \"{pred}\"{m}")

    if success:
        print(f"\n  [RESULT] CATASTROPHIC PRIVACY BREACH")
        print(f"  Secret recovered from packet sizes alone.\n")
    else:
        print(f"\n  [RESULT] ATTACK DEFEATED")
        print(f"  Constant-Size Padding destroyed the signal.\n")
    return success


if __name__ == "__main__":
    run(shield_on=False)
    print()
    run(shield_on=True)
