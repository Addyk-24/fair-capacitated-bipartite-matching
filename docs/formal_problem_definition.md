# Formal problem definition

## Domain-independent allocation model

Let `A={0,...,n-1}` be agents, `R={0,...,m-1}` resources, and `E ⊆ A×R` the
eligible pairs. Each resource `j` has non-negative integer capacity `c_j`; each
eligible edge `(i,j)` has integer utility `u_ij ∈ [0,100]`. The decision variable
`x_ij ∈ {0,1}` denotes assignment of agent `i` to resource `j`.

Feasibility requires:

```text
sum(j : (i,j) in E) x_ij <= 1          for every agent i
sum(i : (i,j) in E) x_ij <= c_j        for every resource j
```

An absent edge is ineligible; an existing edge may have utility zero. Agents need not
all be assigned. The maximum-cardinality objective maximizes `sum_(i,j)∈E x_ij`.
The weighted objective maximizes `sum_(i,j)∈E u_ij x_ij`. The selected fair objective
is lexicographic: maximize assignment count, then maximize the minimum utility among
assigned agents, then maximize total utility. The fairness score is that minimum
divided by 100; an empty allocation has score zero.

## Student–project construction of E and utility

The application converter supplies the graph before optimization. Each project `p`
defines a non-empty list `K_p` of unique named skills. A skill has positive finite
relative weight `q_pk` and a Boolean mandatory flag. Normalize weights by
`w_pk=q_pk/sum_(k∈K_p)q_pk`. Student skills are sets of names. A pair is eligible iff
the student has every mandatory project skill and weighted coverage is at least the
project threshold `t_p` (default 35, valid range [0,100]):

```text
coverage(s,p) = 100 * sum_(k∈K_p) w_pk * 1[k belongs to student s]
(s,p) in E iff all mandatory skills are present and coverage(s,p) >= t_p
u_sp = round_half_up(coverage(s,p))
```

Mandatory skills contribute to coverage. The converter rejects missing or duplicate
project skills, non-positive/non-finite weights, invalid thresholds, and more than
three mandatory skills. Project capacities and student group labels are translated to
common instance metadata. Group labels do not alter the chosen fairness objective.
All algorithms consume only the resulting graph, utilities, and capacities; they do
not contain skill or student/project logic.
