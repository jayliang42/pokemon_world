Pokemon World - developed by OpenGL
========================================

**Building and Running the Lab/Assignment**
===========================================

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
# Pokemon World

This is a small OpenGL exploration game: fly around the field, find roaming
Pokemon, and complete a short research assignment by catching five of them.

**[Play the browser version](https://jayliang42.github.io/pokemon_world/)**

## Build and run

From the project root:

```bash
cmake -S . -B build
cmake --build build -j4
./build/final ./resources
```

If you run the executable from inside `build/`, pass `../resources` instead.

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

## Visual assets

The original Charizard and Umbreon models remain in the project. Bulbasaur is
an original stylized low-poly mesh built from editable primitives for this
repository; it does not contain geometry or textures extracted from a
commercial Pokemon game. Its twelve named OBJ groups provide separate body,
eye, bulb, and leg parts for coloring and animation. Regenerate the checked-in
asset with:

```bash
python3 tools/generate_bulbasaur.py
```

The Charizard model uses the existing UV atlas at `resources/Texture/chariza.png`.
The atlas includes an embedded attribution note to DeliRoko2, which is retained
with the asset.

## Controls

- `W` / `S`: accelerate forward and reverse; releasing the keys brakes smoothly
- `A` / `D`: turn left and right
- `Q` or `Space`: climb
- `E`: descend
- `Z`: toggle gravity and return toward the field
- `1` / `2` / `3`: select Ember, Air Slash, or Flamethrower
- `X`: use the selected move on the locked Pokemon
- `C`: catch the nearest Pokemon in range
- `R`: press twice within three seconds to start a new research run
- `Esc`: quit

You start with ten Poke Balls. Wild Pokemon now have health, species-specific
counter moves, and type effectiveness. Catch five Pokemon to win; using all ten
balls before reaching the goal ends the round. The browser HUD and native
window title show health, progress, and the remaining inventory. Field Research
also tracks a super-effective hit, a defeated wild Pokemon, a gravity-assisted
landing, and the five required captures.

Charizard has a three-move loadout. Ember recovers quickly, Air Slash adds
Flying-type coverage, and Flamethrower trades the longest cooldown for the
highest power. Cooldowns begin only when an attack actually starts, and each
move recharges independently.

The browser build synthesizes lightweight battle, capture, and landing sound
effects with Web Audio after the first player interaction. No commercial game
audio is bundled, and the sound toggle in the page header can mute feedback at
any time.

Progress autosaves after battles, Poke Ball throws, capture results, and safe
landings. The browser build restores its validated save from local storage on
the same browser and device; the native build uses `pokemon_world.save` in its
working directory. Unknown, malformed, or oversized saves are ignored rather
than applied.

Movement uses acceleration, braking, vertical momentum, gravity acceleration,
and a terminal fall speed. The player has a collision radius at the field edge,
slides along a blocked axis, lands at ground level without accumulating downward
velocity, and cannot fly above the world ceiling.
