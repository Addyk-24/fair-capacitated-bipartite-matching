# Fair and Capacity-Constrained Bipartite Matching

**Department of Computer Engineering, Vishwakarma Institute of Technology, Pune**  
**Course:** Design and Analysis of Algorithms (CS3212)  
**Faculty Guide:** Dr. Pushkar Shripad Joglekar  

---

## 1. Project Overview & Objective

This project studies algorithmic allocations of agents to resources subject to:
1. Agent-resource eligibility constraints.
2. Preference/utility metrics on feasible matches.
3. Resource capacities (many-to-one matching).
4. Rigorous, literature-backed fairness criteria across protected agent groups.

The project progresses systematically through four foundational algorithmic stages:
- **Baseline:** Maximum-cardinality bipartite matching.
- **Preference-Aware:** Maximum-weight bipartite matching.
- **Capacity-Constrained:** Min-cost max-flow network formulation.
- **Fairness-Aware Optimization:** Multi-objective or constrained optimization balancing allocation efficiency, capacity limits, and distributional fairness.

---

## 2. Team Members & Roles

| Roll No. | Member Name | Core Ownership Area |
| :---: | :--- | :--- |
| **39** | **Aditya Katkar** | Repo architecture, synthetic data generator, experiment runner & pipeline |
| **31** | **Om Karkele** | Formal problem modeling, cardinality baseline, correctness & complexity[cite: 1] |
| **33** | **Kartik Mandhane** | Utility modeling, maximum-weight matching, utility-fairness integration[cite: 1] |
| **35** | **Yash Kashid** | Capacity modeling, flow-network mapping, capacitated flow experiments[cite: 1] |
| **30** | **Nikhil Kardile** | Fairness literature survey, fairness formulation & metric formalization[cite: 1] |

---

## 3. Repository Structure

```text
fair-capacitated-bipartite-matching/
├── algorithms/             # Core algorithmic implementations (Cardinality, Weight, Flow, Fairness)
├── data/
│   ├── raw/                # Unaltered source/benchmark data
│   ├── processed/          # Sanitized schemas for experiment ingestion
│   └── synthetic/          # Parametrically generated test graphs
├── docs/                   # Formal specs, proofs, literature reviews, and report drafts
├── experiments/
│   ├── configs/            # JSON/YAML parameter sets for batch runs
│   └── runners/            # Automated benchmark harnesses
├── notebooks/              # Exploratory data analysis and prototype verification
├── results/
│   ├── metrics/            # Raw JSON/CSV logs of runtimes, utilities, and fairness values
│   ├── plots/              # Visualizations (Runtime scaling, Pareto trade-offs)
│   └── tables/             # Summary comparison tables
├── src/                    # Shared data structures, validation hooks, IO utilities
├── tests/                  # Pytest test suites (unit, edge-case, and integration tests)
├── CONTRIBUTING.md          # Git branching, commit conventions, and PR review policy
├── pyproject.toml          # Project metadata and configuration
├── requirements.txt        # Production and testing dependencies
└── README.md
