# Multiplayer branch note

The incomplete multiplayer implementation is kept on the separate `steam-frame-multiplayer` branch. It is not part of the solo `steam-frame` source or launcher.

Direct paired play used a Steam Frame host as P1 and a Quest 3 joiner as P2. The cutscene synchronized; only P1 appeared, which the user accepted. P2 ducking or hiding changed P1's perspective. P2 shooting did not work, and no damage was received. The paired gameplay test failed. The cause has not been established.

Further multiplayer work must give P1 and P2 independent health, ammunition, damage, and cover state. One player's duck or cover action must not change the other player's view or cover state, and both players' shots must be able to apply damage to the intended target. Repeat the paired Frame/Quest play test to verify those behaviors before calling multiplayer usable.
