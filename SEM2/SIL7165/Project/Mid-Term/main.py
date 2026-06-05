from generator import PromptGenerator
from llm_query import LLMQueryEngine
from timing_analysis import TimingAnalyzer
from visualize import Visualizer

from tqdm import tqdm


def main():

    generator = PromptGenerator(n_variants=5)

    dataset = generator.generate_dataset()

    engine = LLMQueryEngine()

    analyzer = TimingAnalyzer()

    print("Running experiment...")

    for item in tqdm(dataset):

        prompt = item["prompt"]
        prompt_type = item["type"]

        result = engine.query(prompt)

        analyzer.record(
            prompt_type,
            prompt,
            result["latency"],
            result["blocked"]
        )

    df = analyzer.to_dataframe()

    print("\nSummary Statistics")

    print(analyzer.summary(df))

    stats = analyzer.compute_statistics(df)

    print("\nMean Safe Latency:", stats["safe_mean"])
    print("Mean Jailbreak Latency:", stats["jailbreak_mean"])
    print("Difference:", stats["difference"])

    viz = Visualizer()

    viz.plot_latency_distribution(df)
    viz.plot_prompt_latency(df)


if __name__ == "__main__":
    main()