
# Maximum-Weight Bipartite Matching

## 1. Purpose

This module computes a maximum-total-utility matching between agents and
resources in a bipartite graph.

An agent may be a student or applicant, and a resource may be a project
or job. The algorithm is generic and does not depend on a particular
application domain.

The implementation uses the Hungarian algorithm and supports rectangular
inputs, ineligible pairs, and unmatched agents.

## 2. Input Model

The input graph contains:

- `agents`: unique agent identifiers.
- `resources`: unique resource identifiers.
- `edges`: eligible agent-resource pairs with utilities.

Each eligible edge must have an integer utility in the range 1 through 100.

An agent-resource pair with no edge is ineligible. Duplicate edges for
the same pair are rejected. Edges that refer to unknown agent or resource
identifiers are also rejected.

## 3. Mathematical Formulation

Let:

- A be the set of agents.
- R be the set of resources.
- E be the set of eligible agent-resource pairs.
- w(i,j) be the utility of eligible pair (i,j).
- x(i,j) be 1 if agent i is assigned to resource j, and 0 otherwise.

The objective is to maximize total utility:

    maximize  sum(i,j) in E  w(i,j) * x(i,j)

Subject to:

    sum(j : (i,j) in E) x(i,j) <= 1    for every agent i

    sum(i : (i,j) in E) x(i,j) <= 1    for every resource j

    x(i,j) in {0,1}

Ineligible pairs are not decision variables and cannot be selected.

An agent or resource may remain unmatched. An unmatched agent contributes
zero utility. Therefore, maximizing total utility does not necessarily
maximize the number of assignments.

## 4. Utility Matrix

`buildUtilityMatrix()` creates a matrix with agents as rows and resources
as columns.

- A value from 1 to 100 represents an eligible pair and its utility.
- The value -1 represents an ineligible pair.
- Invalid utilities, duplicate edges, and unknown identifiers cause
  `std::invalid_argument` to be thrown.

The graph's agent and resource lists must contain unique identifiers.

## 5. Algorithm

The implementation proceeds as follows:

1. Build the utility matrix.
2. Orient the matrix so the smaller side of the bipartite graph becomes
   the row set.
3. Add one dummy column for every row. Selecting a dummy column represents
   leaving that row unmatched.
4. Let M be the maximum eligible utility, or zero if no eligible edges
   exist.
5. Convert eligible utilities to costs using:

       cost(i,j) = M - w(i,j)

6. Assign every dummy column a cost of M.
7. Give ineligible real pairs a prohibitive cost.
8. Run the rectangular Hungarian minimum-cost assignment algorithm.
9. Convert real eligible selections into agent-resource assignments.
   Ignore dummy selections and never return an ineligible pair.

Every row can select a dummy column, so a feasible solution never needs
to use an ineligible real edge.

## 6. Why the Cost Transformation Works

Let k be the number of rows in the oriented matrix. Every row receives
exactly one real or dummy column in the augmented assignment problem.

If a row selects an eligible real edge with utility w, its cost is M-w.
If it selects a dummy column, its cost is M.

For a valid assignment, the total cost is:

    total cost = k * M - total utility

Since k and M are constants for the constructed matrix, minimizing total
cost is equivalent to maximizing total utility.

Ineligible real edges have a prohibitive cost. Because every row has an
available dummy column costing at most 100, an ineligible edge is never
needed for an optimal solution.

## 7. Correctness Argument

The Hungarian algorithm finds a minimum-cost assignment for the constructed
rectangular cost matrix.

Each row can be assigned either to an eligible real column or to a dummy
column. Dummy columns represent unmatched rows, while real columns are
unique across the assignment. Thus, selected real pairs form a matching.

Ineligible pairs have a prohibitive cost and can always be avoided by
using a dummy column. Consequently, the returned real assignments are
eligible and satisfy the one-to-one constraints.

By the cost transformation, minimizing the assignment cost maximizes
the total utility of the returned real assignments. Therefore, the
algorithm returns a maximum-total-utility matching.

## 8. Complexity Analysis

Let:

- A = number of agents.
- R = number of resources.
- E = number of eligible edges.
- k = min(A, R).
- l = max(A, R).

### Matrix construction

For each eligible edge, the current implementation searches the agent
and resource lists linearly. This costs O(E(A+R)). Initializing the
utility matrix costs O(AR).

Total matrix-construction time:

    O(E(A+R) + AR)

### Hungarian algorithm

The augmented cost matrix has k rows and l+k columns: the larger original
partition supplies l real columns, and k additional columns represent
unmatched rows.

The rectangular Hungarian algorithm costs:

    O(k^2(l+k)) = O(k^2 l)

because k <= l.

### Overall time

Including matrix construction, cost-matrix preparation, assignment, and
result extraction:

    O(E(A+R) + AR + k^2 l)

### Auxiliary space

The utility matrix requires O(AR) space. The augmented cost matrix
requires O(k(l+k)) = O(AR) space when both partitions are non-empty.
The Hungarian algorithm uses additional O(A+R) working vectors.

Excluding the graph input, the total auxiliary space is:

    O(AR + A + R)

Empty inputs return an empty matching.

## 9. Test Coverage

The unit tests cover:

- A normal 3x3 utility matrix.
- An agent with no eligible resource.
- More agents than resources.
- More resources than agents.
- Ineligible pairs.
- A utility trade-off where greedy selection is suboptimal.
- A case where maximum utility requires leaving an agent unmatched.
- An empty graph.
- A graph with no eligible edges.
- Invalid utility values.
- Duplicate edges.
- Uniqueness of agents and resources in the returned matching.
- Eligibility of every returned pair.

The sample dataset is stored in `data/weighted/small_weighted.csv`.
