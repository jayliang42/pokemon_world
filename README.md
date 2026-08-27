Pokemon World - developed by OpenGL
========================================

**Building and Running the Lab/Assignment**
===========================================

# Pokemon World

This is a small OpenGL exploration game: fly around the field, find roaming
Pokemon, and complete a short research assignment by catching five of them.

> **Unofficial educational portfolio project.** This project is not affiliated
> with or endorsed by Nintendo, Creatures, GAME FREAK, or The Pokemon Company.
> The browser demo does not collect personal or gameplay data; autosave progress
> stays in that browser's local storage.

**[Play the browser version](https://jayliang42.github.io/pokemon_world/)**

**[中文游戏策划设计文档](docs/GAME_DESIGN_DOCUMENT.zh-CN.md)**


All platforms
-------------

Download and extract the lab file [here](/L00.zip):
(<http://users.csc.calpoly.edu/~ssueda/teaching/CSC474/2016W/labs/L00/L00.zip>).

We'll perform an "out-of- source" build, which means that the binary files
will not be in the same directory as the source files. In the folder that
contains CMakeLists.txt, run the following.

	> mkdir build
	> cd build

Then run one of the following, depending on your choice of platform and IDE.

OSX & Linux Makefile
--------------------

	> cmake ..

This will generate a Makefile that you can use to compile your code. To
compile the code, run the generated Makefile.

	> make -j4

The `-j` argument speeds up the compilation by multithreading the compiler.
This will generate an executable, which you can run by typing

	> ./final

!Note this assume a resources directory

To build in release mode, use `ccmake ..` and change `CMAKE_BUILD_TYPE` to
`Release`. Press 'c' to configure then 'g' to generate. Now `make -j4` will
build in release mode.

To change the compiler, read [this
page](http://cmake.org/Wiki/CMake_FAQ#How_do_I_use_a_different_compiler.3F).
The best way is to use environment variables before calling cmake. For
example, to use the Intel C++ compiler:

	> which icpc # copy the path
	> CXX=/path/to/icpc cmake ..

OSX Xcode
---------

	> cmake -G Xcode ..

This will generate `final.xcodeproj` project that you can open with Xcode.

- To run, change the target to `final` by going to Product -> Scheme -> lab3.
  Then click on the play button or press Command+R to run the application.
- Edit the scheme to add command-line arguments (`../../resources`) or to run
  in release mode.

Windows Visual Studio 2015
--------------------------

	> cmake -G "Visual Studio 14 2015" ..

This will generate `final.sln` file that you can open with Visual Studio.
Other versions of Visual Studio are listed on the CMake page
(<https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html>).

- To build and run the project, right-click on `final` in the project explorer
  and then click on "Set as Startup Project." Then press F7 (Build Solution)
  and then F5 (Start Debugging).
- To add a commandline argument (`../resources`), right-click on `final` in
  the project explorer and then click on "Properties" and then click to
  "Debugging."


## Build and run

From the project root:

```bash
cmake -S . -B build
cmake --build build -j4 --target final
./build/final ./resources
```

On macOS the same target also produces `build/Pokemon World.app`, with the game
resources packaged under `Contents/Resources`. It can be launched without a
resource-directory argument:

```bash
open "build/Pokemon World.app"
```

`build/final` remains available for command-line runs and deterministic QA. If
you run that executable from inside `build/`, pass `../resources` instead.

## Browser version on GitHub Pages

The repository also contains a WebAssembly/WebGL2 build. After pushing to the
`main` branch, `.github/workflows/pages.yml` uses Emscripten to build the game
and deploys the generated page to GitHub Pages.

In the repository settings, set **Pages → Build and deployment → Source** to
**GitHub Actions**. The published URL will be:

```text
https://jayliang42.github.io/pokemon_world/
```

The native build remains available for local development. The browser build
uses the same models, textures, movement, and capture rules, but runs its main
loop through the browser animation lifecycle.

Gameplay simulation is platform-independent and advances at a fixed 60 Hz.
Native and browser rendering can run at different frame rates without changing
the player physics, wild Pokemon AI, battle/capture timing, or cooldown clock.
Long frames are bounded so restoring a suspended window or browser tab cannot
trigger an unbounded simulation catch-up.

The Pages workflow also launches the staged Web build in a fresh headless
Chrome profile. Deployment stops unless the game starts with empty browser
storage and reaches the `Autosave ready` state.

Run the native logic tests with:

```bash
ctest --test-dir build --output-on-failure
```

CMake currently registers 36 native test targets.

The suite includes a deterministic ten-minute headless population simulation.
It advances the production Pokemon AI and navigation at 60 Hz for 48 ground and
8 flying Pokemon against the same shared rock, camp, landmark, and regional
spawn layout used by the game. The production day/night encounter pool is part
of the same run: daytime samples expose 5 Umbreon and 24 meadow Pokemon, while
nighttime samples expose 24 Umbreon and 9 meadow Pokemon.
The test checks bounds, physical obstacle separation, persistent crowd overlap,
unexpected Wander/Flee stalls, speed caps, behavior transitions, and both the
Umbreon and aerial Charizard attack-cooldown loops. It runs three distinct seeds
twice each, for 60 simulated minutes in total, and verifies both same-seed
reproducibility and different-seed variation.

A separate full-survey simulation exercises the production capture, battle,
research, recovery, and camp-settlement rules. Four seeded stealth routes finish
five captures with 1-4 Poke Balls remaining and unlock Observer at a score of
750; additional routes cover battle-weakened captures, player faint and camp
recovery, and inventory exhaustion. Every battle and capture sequence advances
at 60 Hz through its real `Finished` phase. Route time uses an explicit modeled
65-second travel budget per target, so it is a resource and rules feasibility
check rather than evidence of an actual new-player completion time.

Native builds also expose deterministic visual-QA captures without changing the
normal game loop. Each command renders the real OpenGL scene, advances the
selected action to a verified active or impact phase, reads the front
framebuffer, and writes a top-down binary PPM image. There are 18 scenarios:

```bash
./build/final ./resources --qa-capture /tmp/camp.ppm --qa-scenario camp
./build/final ./resources --qa-capture /tmp/umbreon.ppm --qa-scenario umbreon
./build/final ./resources --qa-capture /tmp/charizard.ppm --qa-scenario charizard
./build/final ./resources --qa-capture /tmp/trails.ppm --qa-scenario trails
./build/final ./resources --qa-capture /tmp/moonshadow-survey.ppm --qa-scenario moonshadow-survey
./build/final ./resources --qa-capture /tmp/redrock-survey.ppm --qa-scenario redrock-survey
./build/final ./resources --qa-capture /tmp/alpha-nest.ppm --qa-scenario alpha-nest
./build/final ./resources --qa-capture /tmp/alpha-capture.ppm --qa-scenario alpha-capture
./build/final ./resources --qa-capture /tmp/landmarks.ppm --qa-scenario landmarks
./build/final ./resources --qa-capture /tmp/ecology-day.ppm --qa-scenario ecology-day
./build/final ./resources --qa-capture /tmp/ecology-night.ppm --qa-scenario ecology-night
./build/final ./resources --qa-capture /tmp/capture-aim.ppm --qa-scenario capture-aim
./build/final ./resources --qa-capture /tmp/capture-hit.ppm --qa-scenario capture-hit
./build/final ./resources --qa-capture /tmp/ember.ppm --qa-scenario ember
./build/final ./resources --qa-capture /tmp/air-slash.ppm --qa-scenario air-slash
./build/final ./resources --qa-capture /tmp/flamethrower.ppm --qa-scenario flamethrower
./build/final ./resources --qa-capture /tmp/perfect-dodge.ppm --qa-scenario perfect-dodge
./build/final ./resources --qa-capture /tmp/cover-blocked.ppm --qa-scenario cover-blocked
```

`camp` captures the ordinary starting view. `capture-aim` holds the production
aim state at 72% charge and asserts a locked target plus a valid prediction path;
the view shows camera-facing arc markers, a ground-distance projection, and the
predicted landing marker. `capture-hit` releases a minimum-charge throw, advances
the production swept collision at 60 Hz, requires a successful target hit, and
captures the real absorbing phase without writing the QA throw to the player's
save. The player-move scenes assert that their production collision volumes
actually hit the target; `perfect-dodge` passes through the real controller
invulnerability and timing checks, while `cover-blocked` requires an actual
obstacle impact. These capture paths are Native-only; browser builds retain
their existing startup and main-loop flow.

`alpha-nest` completes both prerequisite regional surveys, uses the production
`F` interaction to activate the encounter, and verifies the Alpha-only radar,
soft lock, HUD contract, enlarged model, nest marker, and readable framing.
`alpha-capture` weakens the Alpha, completes a deterministic production capture,
and verifies that the resolved state is saveable without incrementing the
ordinary caught or defeated survey counters.

`ecology-day` and `ecology-night` use the same camera and deterministic spawn
slots, making the encounter-pool change directly comparable instead of relying
on a color-only lighting difference.

`trails` shows the full camp fork from a single elevated camera. The cool grey
route reaches Moonshadow Edge and its track-observation marker; the warm brown
route climbs to Redrock Highlands and its lookout marker. A cyan marker
identifies the shared trailhead. Moonshadow tracks can be recorded after landing
at Twilight or Night, while the Redrock lookout can be surveyed after landing at
any time. Both use the production `F` interaction, award one research objective,
autosave immediately, and change to a stable blue-white completed marker.
`moonshadow-survey` and `redrock-survey` exercise that full interaction path,
reject duplicate credit, validate the shared HUD state, and capture the completed
site from its actual region.

The field also runs a five-minute day/night cycle. The sky, sun, ambient light,
fog, terrain, and Pokemon use the same lighting sample each frame, so dusk and
night reduce visibility consistently instead of only recoloring the background.
The same continuous daylight value also drives species ecology. Eevee and
Bulbasaur wander more actively by day, while Umbreon sees and hears farther,
builds alertness faster, and stays alert longer at night. Native and Web HUD
summaries label the current Day, Twilight, or Night ecology state. Deterministic
slot presence makes Eevee and Bulbasaur common by day and Umbreon common at
night. Dormant slots retain capture and health state, but are excluded from
targeting, radar, collision, alerts, projectiles, encounters, and rendering;
an active battle or capture target remains present until the interaction ends.

The field now has two directional landmark families beyond the central camp.
Dark teal moon trees gather across the forward-left ridge, while warm redrock
spires and amber crystals mark the forward-right highland. Their shared layout
also drives player collision, Pokemon navigation, sightline and battle cover,
and Poke Ball prediction, so the landmarks are physical parts of the world.
The `landmarks` QA scene starts in the air with both silhouettes visible.

A shared nine-segment trail network now turns those silhouettes into readable
routes. Both branches begin at the camp trailhead, conform to the real height
field, and use region-specific material tones before ending at observation
markers; the Redrock route continues to the Alpha nest. The layout and shader
consume the same segment coordinates, while tests keep each branch continuous
and every marker attached to the network.

The same field now has three continuous habitat surfaces rather than one grass
material. Windwhisper Meadow stays bright around the camp, Moonshadow Edge fades
into cool blue-green ground beneath the trees, and Redrock Highlands transitions
to warm rust soil around the spires. Shared `WorldRegion` data drives both these
shader boundaries and deterministic initial habitats: Eevee and Bulbasaur begin
in the meadow, Umbreon around the forest edge, and wild Charizard over the
highlands. Region tests keep the blend normalized and the camp spawn exclusion
intact.

## Visual assets

The original Charizard and Umbreon models remain in the project. Bulbasaur and
Eevee are original stylized low-poly meshes built from editable primitives for
this repository; they do not contain geometry or textures extracted from a
commercial Pokemon game. Their named OBJ groups provide separate body, eye,
tail, ear, and leg parts for coloring and animation. Regenerate the checked-in
assets with:

```bash
python3 tools/generate_bulbasaur.py
python3 tools/generate_eevee.py
python3 tools/generate_camp.py
python3 tools/generate_landmarks.py
```

The Charizard model uses the existing UV atlas at `resources/Texture/chariza.png`.
The atlas includes an embedded attribution note to DeliRoko2, which is retained
with the asset.

## Controls

- `W` / `S`: accelerate forward and reverse; releasing the keys brakes smoothly
- `A` / `D`: turn left and right
- `Q` or `Space`: climb
- `E`: descend
- `Shift`: dodge forward; time it against a wild Pokemon's incoming attack
- `Z`: toggle gravity and return toward the field
- `1` / `2` / `3`: select Ember, Air Slash, or Flamethrower
- `X`: use the selected move on the locked Pokemon
- Hold `C`: aim and charge a Poke Ball while the prediction arc is visible
- Release `C`: throw along that arc; the soft lock highlights a target but does not guarantee a hit
- `F`: interact while grounded in a marked area—use the field camp, record either regional survey, or activate the unlocked Alpha nest
- `L`: deploy a field lure after Observer rank unlocks; lures require a grounded, active survey away from camp
- `R`: press twice within three seconds to start a new research run
- `Esc`: quit

You start at a physical field camp with ten Poke Balls. Wild Pokemon have
health, species-specific counter moves, and type effectiveness. Catch five
Pokemon, return to the marked camp landing circle, land, and press `F` to submit
the survey. Submission records a research score, restores Charizard, and
restocks ten Poke Balls. Using all ten balls before reaching the goal ends the
round. The browser HUD and native
window title show health, progress, and the remaining inventory. Field Research
also tracks three species-specific tasks—catching an unhurt Eevee, observing a
Bulbasaur flee, and observing Umbreon's territorial warning—alongside a
super-effective hit, a defeated wild Pokemon, a gravity-assisted landing, and
the five required captures. Recording Moonshadow Tracks and surveying Redrock
Lookout bring the checklist to nine objectives. Each objective grants credit at
most once per survey and contributes to the camp settlement score.
Submitting a survey worth at least 700 points permanently advances the save from
Trainee to Observer rank. Observer surveys start with two field lures. A deployed
lure lasts 14 seconds, draws a calm Eevee from within 18 metres, and improves
capture probability for Pokemon within 3.5 metres. Lure inventory is consumed
and autosaved when placed, while the active scent itself is intentionally
limited to the current run.

The camp is a real world-space location with a tent, workbench, supply crate,
flag, collision, a safe radius, and a landing marker. Its HUD beacon replaces
the Pokemon radar while Charizard is at camp or ready to submit. Capturing the
fifth sample no longer freezes the run: the player must navigate back and land
before the result can be submitted.

The browser HUD also includes a Field Radar. It continuously points to the
nearest uncaught, non-fainted research sample and reports its field distance,
so exploration remains directed even when the target is outside the camera view.

Completing both regional surveys unlocks the crimson Alpha nest in Redrock
Highlands. Land inside its marker and press `F` to summon an enlarged Alpha
Charizard. While the encounter is active, the Field Radar and soft lock reserve
priority for that Alpha. It can be resolved through battle or capture; the
result is tracked separately from the ordinary caught and defeated survey
counters. The resolved nest persists across saves, while an active unfinished
encounter intentionally does not.

Capturing now uses a physical projectile instead of an automatic lock-on hit.
Short and fully charged throws follow different ballistic arcs. Every fixed
simulation step sweeps the Poke Ball against Pokemon, terrain, and boulders, so
a target can be missed and cover can intercept the throw. The ball is consumed
when released; only an actual Pokemon hit proceeds to the capture probability
and shake sequence.

Charizard has a three-move loadout. Ember is a precise 14-metre projectile,
Air Slash uses a wider 20-metre projectile, and Flamethrower is a short
10.5-metre cone that trades the longest cooldown for the highest power. The
attack volume is swept against the target, terrain, boulders, and solid camp
structures. A miss or blocked attack deals no damage, ends at the real impact
point, and still gives the wild Pokemon its counterattack. The Web move HUD
shows each move's shape and range. Each move also has its own startup, active,
recovery, hit-stagger, and movement-lock profile: Ember stays mobile, Air Slash
requires a moderate commitment, and Flamethrower locks ordinary movement until
its long recovery ends. Release position, aim, collision, and counter eligibility
are resolved when the active phase begins, after startup movement has occurred.
Cooldowns begin only when an attack actually starts, and each move recharges
independently.

Dodging has its own cooldown and a short invulnerability window. Wild attacks
lock their impact point when the projectile launches, so a well-timed dodge can
avoid damage by leaving that area. Dodge movement still obeys the player's
collision radius, field boundary, terrain height, and boulder collision. A
successful dodge started 0.08–0.32 seconds before impact is perfect: after the
exchange, the HUD opens a 1.6-second counter window on that attacker. The next
ready move starts 45% faster and deals 35% more damage; attacking, timing out,
losing the target, or entering another encounter clears the opportunity.

Wild species now have distinct field behavior. Bulbasaur remains timid and
flees at close range, while Umbreon guards its territory: it pauses to warn the
player, pursues inside its alert radius, and can initiate Bite without waiting
for the player to attack. Amber and red ground rings plus the Wild Alert HUD
show the escalation, and Shift can evade the telegraphed Bite. Bite uses a short
model lunge and local jaw impact instead of a detached projectile arc. A wild
Charizard now warns before beginning an aerial attack run, circles at an
11–16-metre standoff distance while aligning altitude, and launches Wing Attack
only inside its vertical engagement band. It returns to a longer watchful
cooldown after each pass.

Wild counter moves use the same world collision resolver as Charizard's move
loadout. Bite and Tackle are short melee lunges, Vine Whip is a narrow line, and
Wing Attack is a wide 18-metre projectile. Each has its own danger radius. The
attack locks its destination when the active phase starts and ends at the first
player, terrain, boulder, or camp collision. Cover therefore prevents damage,
and the danger ring, effect, status message, player recoil, and Web sound all
reflect the actual impact point.

Vision now follows the world instead of passing through it. Terrain ridges,
boulders, and the solid camp structures can interrupt a Pokemon's view of the
player, while close movement remains audible through cover. When one ground
Pokemon becomes alert, its warning can spread to a calm same-species companion
within 11 metres only when the two have a clear sightline. A 2.5-second global
cooldown prevents one warning from cascading across the field in a single
update, and the game status plus Web audio distinguish a group warning from a
single encounter.

The browser build synthesizes lightweight battle, dodge, capture, and landing
sound effects with Web Audio after the first player interaction. No commercial
game audio is bundled, and the sound toggle in the page header can mute feedback
at any time.

Progress autosaves after battles, Poke Ball throws, capture results, safe
landings, regional observations, and Alpha resolution. The browser build
restores its validated save from local storage on
the same browser and device; the native build uses `pokemon_world.save` in its
working directory. Unknown, malformed, or oversized saves are ignored rather
than applied.

Save payloads use the explicit `PW_SAVE_V6` schema header. V6 persists whether
the Alpha nest was resolved; the transient active encounter is deliberately not
saved. V5 persists the two one-time regional observations, V4 persists research
rank and remaining lure inventory, V3 added the three species research tasks,
and V2 recorded whether a survey was submitted. The parser migrates valid V1
through V5 saves into the current model. A submitted V3 survey is evaluated once
during migration so a qualifying score can unlock Observer rank; older saves
start new fields at safe defaults. An old completed V1 run remains completed
while an unfinished run remains active. Malformed,
unsupported older, and newer versions fail closed. Future versions must retain
their old parser and add a tested migration into the current `GameSaveData`
model.

Movement uses acceleration, braking, vertical momentum, gravity acceleration,
and a terminal fall speed. The player has a collision radius at the field edge,
slides along a blocked axis, lands at ground level without accumulating downward
velocity, and cannot fly above the world ceiling.
