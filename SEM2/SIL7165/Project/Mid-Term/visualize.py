import matplotlib.pyplot as plt


class Visualizer:

    def plot_latency_distribution(self, df):

        safe = df[df["type"] == "safe"]["latency"]
        jailbreak = df[df["type"] == "jailbreak"]["latency"]

        plt.figure()

        plt.hist(safe, bins=10, alpha=0.6, label="Safe Prompts")
        plt.hist(jailbreak, bins=10, alpha=0.6, label="Jailbreak Prompts")

        plt.xlabel("Response Time (seconds)")
        plt.ylabel("Frequency")

        plt.title("LLM Response Time Distribution")

        plt.legend()

        plt.show()

    def plot_prompt_latency(self, df):

        plt.figure()

        plt.scatter(range(len(df)), df["latency"])

        plt.xlabel("Query Index")
        plt.ylabel("Latency (seconds)")

        plt.title("Response Latency per Prompt")

        plt.show()