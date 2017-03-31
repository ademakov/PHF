# PHF

A simple C++ perfect hash function generator. It is based on the ideas
from this paper:

* Antoine Limasset, Guillaume Rizk, Rayan Chikhi, Pierre Peterlongo.
[Fast and scalable minimal perfect hashing for massive key sets](https://arxiv.org/abs/1702.03154)

The authors of the paper provided their own implementation: [BBHash](https://github.com/rizkg/BBHash).

My implementation provides a different interface and features, It
is not supposed to handle massive key sets. Rather it is oriented
to getting fast and simple lookup tables for moderate key sets.
