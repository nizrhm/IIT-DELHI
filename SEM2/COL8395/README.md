# COL8395: Special Topics in Databases / Vector Search DBMS

This directory contains research presentations and project work for **COL8395: Special Topics in Database Management Systems** at IIT Delhi. The focus is on Approximate Nearest Neighbor (ANN) search, high-dimensional vector databases, and indexing optimizations for AI-driven search applications.

---

## 📁 Course Structure

### 🔍 [PROJECT: Adaptive Vector DBMS using FAISS](./PROJECT/)
* **Objective:** Design and implement a high-performance Vector DBMS showcasing vector quantization techniques (PQ, OPQ, AQ) with a custom **Adaptive Reranking** optimizer.
* **Details:**
  * Uses an Inverted File Index (IVF) to cluster high-dimensional vectors and narrow the search space.
  * Implements Product Quantization (PQ), Optimized PQ (OPQ), and Additive Quantization (AQ) to compress vector dimensions.
  * Evaluates accuracy and latency trade-offs on classic benchmarks: MNIST (digits), SIFT (image descriptors), GloVe (word embeddings), and Deep1B (large-scale image features).
  * **Adaptive Reranking:** Dynamically increases the candidate reranking pool for ambiguous queries (based on the score gap between the top two results) to maintain low search times while maximizing overall recall accuracy.
  * **Go to Project README:** For implementation details, script parameters, and running instructions, see [PROJECT/README.md](./PROJECT/README.md).

### 📊 [PRESENTATIONS](./PRESENTATIONS/)
* **FILIP (Fine-grained Interactive Language-Image Pre-training):** Research slides reviewing fine-grained cross-modal search structures (`FILIP By Nizaul.pdf`).
* **Tensor-Based Quantization (TB-Quant):** Presentation on vector quantization optimizations for fast retrieval in high-dimensional spaces (`TB_QUANT_NIZAUL.pdf`).

---
[← Semester 2 Index](../README.md) | [← Portfolio Root](../../README.md)
