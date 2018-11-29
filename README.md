# malloclab
A malloc implementation that was done in Spring 2018.

I've used an explicit list using a binary tree.
Here, we don't have a fancy segregated list of different sizes. However, I utilized the benefits of BT structure in order to order the blocks in the free list, so that the lookup time could be minimized.

Received a total score of 98% (93%+5% bonus)
