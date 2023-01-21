# Amnesia: The Dark Descent
[![CI](https://github.com/shamazmazum/AmnesiaTheDarkDescent/actions/workflows/build.yml/badge.svg)](https://github.com/shamazmazum/AmnesiaTheDarkDescent/actions/workflows/build.yml)

This is a port of Amnesia: The Dark Descent to Linux/BSD systems. It's 100%
binary blob free (thanks to buzer2020's port). Only few dependencies are bundled
with this fork:

* OALWrapper. It's easier to bundle it because it's specific to Frictional
  Games.
* Newton Dynamics Engine. Finding the sources for this particular version was a
  pain (again, thanks to buzer2020 here on GitHub).
* AngelScript. There is `lang/angelscript` port in FreeBSD ports collection, but
  that version is too new for this game.

# Branches
- origin/master - contains a graphics backend rework using [BGFX](https://github.com/bkaradzic/bgfx)
- origin/original - orignal running code should be as bug free as when the game was released

## Building

Just run

~~~~
mkdir build && cd build
cmake ../amnesia/src
make
~~~~

or, alternatively, if you want your game assets installed system-wide:

~~~~
mkdir build && cd build
cmake -DSYSTEMWIDE_RESOURCES=ON -DSYSTEMWIDE_RESOURCES_LOCATION="/path/to/game/assets" ../amnesia/src
make
~~~~

This will produce a single executable, `Amnesia` which you need to run to play
the game.

## Dependencies

In addition to those ancient bundled dependencies, the game needs a few newer
ones:

* OpenGL
* SDL2
* libtheora
* libvorbis
* libvorbisfile
* DevIL
* GLEW

All this libraries can be found in FreeBSD ports.

## Additional notes

The original repository has a bunch of additional tools like level and model
editors. Because these tools are not suitable for my Wayland environment and
require X libraries, I do not build them. The sources are still here, however.
