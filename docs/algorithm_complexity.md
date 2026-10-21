# Algorithm objectives and complexity

Let `A` be agents, `R` resources, `E` eligible edges, and `F` the maximum feasible
flow (at most `min(A, sum(capacity))`). The bounds below exclude reading the input.

| Algorithm | Objective and capacity semantics | Time | Auxiliary space |
| --- | --- | --- | --- |
| `cardinality` | Maximum number assigned; capacity-one matching, any positive resource capacity is treated as one and zero is unavailable | `O(AE)` augmenting paths | `O(A + R + E)` |
| `weighted` | Maximum total utility with capacity-one resources | `O(E(A+R) + AR + k²l)`, `k=min(A,R)`, `l=max(A,R)` | `O(AR + A + R)` |
| `capacitated_flow` | Maximum total utility under full capacities | `O(VE + F E log V)` with potentials; `V=A+R+2`, residual `E'=O(E+A+R)` | `O(V + E')` |
| `capacitated_flow_maxcard` | Maximum assigned count, then maximum total utility under full capacities | `O(VE + F E log V)` with potentials | `O(V + E')` |
| `fairness` | Maximum assigned count, then maximum minimum assigned utility, then maximum total utility as tie-break; full capacities | `O(log E · (VE + F E log V))` using the shared flow solver for threshold checks | `O(V + E)` |
| `greedy_reference` | Heuristic by descending utility, full capacities | `O(E log E)` | `O(E)` |

The fairness implementation sorts distinct usable edge utilities and binary-searches
the largest threshold that still admits a maximum-cardinality flow. The existing
min-cost max-flow implementation checks each threshold and applies the documented
total-utility tie-break on edges at or above the selected threshold. Empty assignments
have fairness score zero; otherwise the score is the minimum assigned utility divided
by 100.

For comparisons, `weighted` and `cardinality` directly match the capacity-aware
problem only when every resource capacity is one. Result tables suppress their ratios
outside that setting. The fairness algorithm is compared on coverage and minimum
assigned normalized utility, not against the total-utility optimizer as though the
objectives were interchangeable.

All edge utilities accepted by the common instance validator are integers in `[0,100]`.
An existing zero-utility edge is eligible; absence is represented only by a missing edge
(and by `-1` in the weighted solver's internal matrix). The student–project converter
normalizes positive per-project skill weights and constructs the graph before any
matching algorithm runs; its skill scans take `O(A·P·S)` time for `A` students, `P`
projects, and at most `S` skills per project.
