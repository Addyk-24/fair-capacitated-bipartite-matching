# Maximum-Weight Bipartite Matching

## 1. Purpose

The maximum-weight bipartite matching stage extends the basic
maximum-cardinality matching problem by assigning a utility value to
each eligible agent-resource pair.

The objective is to select a one-to-one set of assignments that
maximizes the total utility.

---

## 2. Input Representation

The input is represented using a weighted bipartite graph:

- Agents represent the left-side vertices.
- Resources represent the right-side vertices.
- An edge represents an eligible agent-resource pair.
- Each edge has an integer utility value.

The implementation uses a utility scale of 1 to 100.

An absent edge represents an ineligible agent-resource pair.

---

## 3. Internal Representation

The weighted graph is converted into a utility matrix.

For example:

|      | R1 | R2 | R3 |
|------|----|----|----|
| A1 | 10 | 6 | 2 |
| A2 | 8 | 9 | 4 |
| A3 | 5 | 7 | 10 |

An internal value of `-1` represents an ineligible pair.

---

## 4. Algorithm

The implementation uses the Hungarian algorithm.

The Hungarian algorithm is naturally formulated as a minimum-cost
assignment algorithm. Since this project maximizes utility, utilities
are converted to costs using:

    cost(i,j) = maximum_utility - utility(i,j)

Ineligible pairs are assigned a very large cost so that they are not
selected.

The algorithm then produces a one-to-one assignment between agents
and resources.

---

## 5. Output

The algorithm returns a `WeightedMatchingResult` containing:

- Selected assignments
- Number of assigned agents
- Total utility
- Runtime in milliseconds

For the sample dataset:

    A1 -> R1 = 10
    A2 -> R2 = 9
    A3 -> R3 = 10

Therefore:

    Assigned count = 3
    Total utility = 29

---

## 6. Correctness Testing

The implementation has been tested using five cases:

1. Normal 3x3 weighted matching
2. Agent with no eligible resource
3. More agents than resources
4. Ineligible/missing edges
5. Utility trade-off requiring global optimization

All five tests pass.

---

## 7. Time Complexity

For an n x n assignment matrix, the Hungarian algorithm has:

    Time complexity: O(n^3)

where `n` is the number of agents/resources after converting the
problem to the required assignment form.

For rectangular inputs, the implementation handles the unequal
dimensions by transposing the matrix when there are more agents than
resources.

---

## 8. Space Complexity

The utility/cost matrix requires:

    O(n^2)

space.

The Hungarian algorithm also maintains auxiliary arrays of size
proportional to the matrix dimensions.

Therefore, the overall space complexity is:

    O(n^2)

---

## 9. Relationship to the Project Algorithm Progression

The maximum-weight stage extends the maximum-cardinality baseline.

Maximum-cardinality matching focuses on maximizing the number of
assignments.

Maximum-weight matching additionally considers the utility of each
eligible assignment and maximizes total utility.

The later capacitated and fairness-aware stages build on this
allocation framework.