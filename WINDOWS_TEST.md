# Windows test checklist

Build with MSVC (`cmake --build build/gta_sa --config Release --parallel 16`). `gta` opens classic, `gta --rewrite` opens rewrite.

- [ ] Sidewalk collisions: walk along curbs and thin edges. Any sticking?
- [ ] Swim out: swim to a ledge and jump. Does CJ climb out and animate after?
- [ ] Lay flat after a climb: canal vault at 2624.64, -1948.57 with W held. If it still happens, run with `GTA_CJLOG=1` and keep the log.
- [ ] Trackpad and window on Wayland / Hyprland over VNC.
- [ ] Spanish: Options > Language > Spanish; check menus and HUD. Check the French, German and Italian wording too.
- [ ] Display page: Brightness, Radar Mode, HUD Mode, MIP Mapping, Anti Aliasing (2x, 4x: check screenshots with F5 still work), Restore Defaults.
- [ ] Controller: menu navigation, weapon wheel on LB, analog movement, tap A to sprint.
- [ ] Effects: water splashes, landing dust, sprint FOV kick (rewrite only).
- [ ] Guns: backtick freecam, check every gun for finger clipping and tilt. Report which ones.
- [ ] Vault drop pose: vault a fence or canal edge and drop. CJ should fall upright, never in the bind pose.
- [ ] Sounds: splash on entering water, landing thud after a fall (rewrite and classic).
- [ ] Gun barrels point along the aim line in third person (silenced pistol, sawn-off, rifles).
- [ ] Map legend: Options > Display > Map Legend ON, open the map, set a waypoint. A panel lists the waypoint and CJ.
- [ ] Wayland: window size and resize behaviour with `scripts/run_wine.sh` notes (Linux/Wine only).
