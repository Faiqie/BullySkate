# Validation and beta limits

The public packaging change has been checked against the original private installation. Local conversion reproduces the same Bully world collision, grind rails and Jimmy bind-rig files. Native car bounds are now loaded from locally generated data instead of compiled into the ASI. No source physics solve or active rig-retarget logic was replaced for packaging.

Previous isolated checks exercised the worker protocol, 600 source frames, ollies, remounts, 24 NPC snapshots, variable input rates, preferences, all 37 gesture choices, FOV, markers, busy queues, towing and board-hit production. The native reaching pose preserved measured arm lengths and landed on the tested rear-panel target. D3D state/reset and rig restoration checks passed. The original user confirmed working native display and Alt+F4.

The async adapter benchmark is a CPU timing measurement, **not whole-game FPS**. Final FPS, visual rig appearance, controller feel, board strikes and car behavior across all areas still need gameplay feedback. Wheel meshes move with the deck without separate spinning. NPC capsules do not replace native ragdolls. Other moving props are not mirrored. The supplied rewrite is incomplete and has two known failing upstream core tests. Every mission, clothing combination, interior and transition has not been tested.

This release is labeled beta so others can test these limits openly. `release-validation.json` records the packaging/setup checks actually run for the uploaded build. No game files or private path-containing transcripts are included as evidence.
