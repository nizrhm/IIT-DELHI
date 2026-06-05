from collections import defaultdict
import math

class AITokenReconstructor:
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
                    next_word = words[i+1]
                    self.bigrams[(word, next_word)] += 1
                    
    def _score_bigram(self, w1, w2):
        count_w1 = self.unigrams.get(w1, 0)
        count_bigram = self.bigrams.get((w1, w2), 0)
        prob = (count_bigram + 1) / (count_w1 + len(self.unigrams))
        return math.log(prob)
        
    def infer_sentences(self, lengths: list, top_n=3):
        print(f"[*] AI Length Filter: Attempting to match signature {lengths}")
        
        candidates = []
        for L in lengths:
            words_of_length = list(self.length_to_words.get(L, []))
            if not words_of_length:
                return ["<INSUFFICIENT DATA: Unknown word length intercepted>"]
            candidates.append(words_of_length)
            
        paths = [(math.log((self.unigrams[w] + 1) / (self.total_words + len(self.unigrams))), [w]) for w in candidates[0]]
        
        for i in range(1, len(lengths)):
            new_paths = []
            for next_word in candidates[i]:
                # We want to keep all valid paths, not just the single best one per word, 
                # so we can output top N guesses at the end.
                for score, prev_path in paths:
                    prev_word = prev_path[-1]
                    transition_score = self._score_bigram(prev_word, next_word)
                    total_score = score + transition_score
                    new_paths.append((total_score, prev_path + [next_word]))
            
            # Prune to avoid combinatorial explosion, keep top 100 paths
            new_paths.sort(key=lambda x: x[0], reverse=True)
            paths = new_paths[:100]
            
        paths.sort(key=lambda x: x[0], reverse=True)
        return ["".join(path) for score, path in paths[:top_n]]
