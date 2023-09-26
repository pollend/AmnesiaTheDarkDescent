# Amnesia: The Dark Descent
[![CI](https://github.com/pollend/AmnesiaTheDarkDescent/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/pollend/AmnesiaTheDarkDescent/actions/workflows/build.yml) [![Itch.io](https://img.shields.io/badge/Itch-%23FF0B34.svg?style=for-the-badge&logo=Itch.io&logoColor=white)](https://mpollind.itch.io/amnesia-the-dark-descent-redux)

This is a heavy rework of the core engine, mostly an experiment in absurdity to see what can be restructured and improved. Note that this is still a work in progress, so a lot of stuff will be broken. It's far enough along that it's possible to play through the entire game. Currently, there are a lot of rendering artifacts, broken functionality, and various other problems that shouldn't be too hard to run into. A lot of bug fixing is needed before this gets to a usable state.

![Preview](./imgs/shot1.png?raw=true)

## Current Features

- Graphics backend rework in The Forge
  - removed bookkeeping for the viewport size, so it's possible to resize the window and have all the window frame buffers update.
  - Dropped usage of the OpenGL fixed function pipeline.
  - replaced the exsiting SSAO implementation with [HBAO+](https://www.nvidia.com/en-gb/geforce/technologies/hbao-plus/technology/)
  - fixed parallax occlusion mapping, rendering artifacts need to be fixed with curved surfaces.
- hpl::Event is an interface that makes building Observables a lot easier. 
- RTTI interface avoid the overhead of using dynamc_cast<>
## Random Ideas

- Replace newton physics with Jolt [ticket](https://github.com/pollend/AmnesiaTheDarkDescent/issues/20)
- Remove OALWrapper and use miniaudio [ticket](https://github.com/pollend/AmnesiaTheDarkDescent/issues/13)
- Remove iXMLDocument indirection and use FastXML [ticket](https://github.com/pollend/AmnesiaTheDarkDescent/issues/25)
- Would like to mess around with raytracing support from Vulkan and DirectX, would probably have to use another solution. 
- Various collection of possible improvements to the editor [ticket](https://github.com/pollend/AmnesiaTheDarkDescent/issues/22)

Feel free to drop a ticket if your interested in anything. 

# Branches
- origin/master - Vulkan/Direct11/Direct12 implementation using [The-Forge](https://theforge.dev/)
- origin/original - original running code should be as bug free as when the game was released

## Building

~~~~
mkdir build && cd build
cmake .. -DAMNESIA_GAME_DIRECTORY:STRING='' -G "Ninja/Visual Studio 17 2022"
make
~~~~

or, alternatively, if you want your game assets installed system-wide:

~~~~
mkdir build && cd build
cmake -DSYSTEMWIDE_RESOURCES=ON -DSYSTEMWIDE_RESOURCES_LOCATION="/path/to/game/assets" ../amnesia/src
make
~~~~

Check the Github actions for additional guidance if you're unable to build. there is also the [Frictional Discord](https://discord.com/invite/frictionalgames), I should be available for support under #hpl-open-source.

## Dependencies

This repository comes prepackaged with pretty much all the dependencies that are needed to compile the engine. Unless instructed
otherwise from vcpkg.

The main things that need to be installed and are not included are `python3` and `perl. For Windows just makes sure these dependencies are included 
on the system path. 


# License

- Any code published by `Frictional` is under a GNU GENERAL PUBLIC LICENSE
- Some code published by `Open 3D Engine` which is under an Apache-2.0 OR MIT
- Any new code that is under my name will use an Apache-2.0 LICENSE 
