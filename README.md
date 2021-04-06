# handmade

## State of the engine
I had two repositories before but now all code for the enginge just lives here in the Code folder for simplicitys sake.

The repository is named after the quite amazing [Handmade Hero](https://handmadehero.org/) web series
which I am working my way through (_ep. 240~_), learning how to make this stuff. Everything in the repository is written by hand by me apart from a couple of functions in math.h that I will revisit once I start optimizing things.

Most recently I added a rigid body 3D physics system and started on multithreaded asset-streaming. This temporarily broke alot of things like glyph generation and since I haven't fixed that yet and since I removed the demo code for the physics part and changed it to a game based on moving around a alphabet grid the demo looks quite boring right now.

I have now learned the hard way that it's best to work with releases even with personal projects and that it is important to leave a trail of working executables.

## How to compile
1. `git clone https://github.com/Bjeeeern/handmade`
2. `cd handmade`
3. `git submodule update --recursive --init data`
4. `call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64`
5. `code\build.bat`
6. `code\run.bat`

## How to run release
1. Download and unpack zip
2. run `build\win32_game.exe`

----

### Old Readme

Below is an example of the engine loading and unloading entities as the camera move around. The world is an "as large as you want" hash mapping from world coordinates to blocks of entities. When entities move about they jump from block to block.

<img src="promo_data/Handmade.gif" width="400" /> <img src="promo_data/HandmadeZoomedOut.gif" width="400" />


I have followed along until episode 59 on and off and am now currently adding a car and some physics to figure out what I want the game to be like. I am currently using Minkowski sum to make it a bit easier to think about rotated rectangles. 

<img src="promo_data/MinkowskiSum.gif" width="400" /> <img src="promo_data/CarMovement.gif" width="400" />


I did some detouring along the way, playing around with 3D cameras and font file processing. I ended up with my own font format that just calculates all the glyphs for monospaced fonts and dumps them as quadratic Bézier curves. More usable for editors but bad for user-facing rendering.

<img src="promo_data/3DCamera.gif" width="400" /> <img src="promo_data/FontIsWorkingJapaneseToo.gif" width="400" />

(If you see no gifs turn off your adblock for github!)
