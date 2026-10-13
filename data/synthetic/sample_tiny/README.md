# sample_tiny

Hand-made 3-agent, 2-resource instance (capacities 1 and 2) for checking algorithms by hand.

| Allocation | Assigned | Total utility |
| :--- | :--- | :--- |
| Greedy by utility (`greedy_reference`) | 0→0, 1→1 | 17 |
| Optimal (maximum weight) | 0→1, 1→1, 2→0 | 5 + 7 + 9 = **21** |
| Maximum cardinality | all 3 agents assigned | 3 |

Greedy grabs the 10-utility edge first, which blocks agent 2 from resource 0 and costs
one assignment. Any correct weighted or capacitated algorithm should reach 21 with
3 assigned agents.
