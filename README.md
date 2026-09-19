# cwordle

Here's the idea: Levenshtein edit distance.

It's much easier to remember a large amount of words if there is a simple way to move between them. Ideas include chains, trees, graphs.

I think I'll try trees first, with an initial set of 31 words. We'll have 1 starting word, and 5 children, and each child will have 5 children.

The first step is to collect data on what you're most likely to see when guessing yesterdays word as the first word for today. Then using that data along with swordle, I'll construct the best possible guesses and string them together into a tree structure.

