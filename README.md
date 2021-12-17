DOOM-CLR
========

This is DOOM runnable as a .NET application.

I took DOOM-CRT by Mattias Gustavsson as a baseline because I really
like the way it builds (run `cl doom.c` and off you go). Readme for
DOOM-CRT follows this one.

DOOM-CLR has a couple patches so that it can be built with the `/clr` switch.

Common type system doesn't have a representation for "method with an
unknown signature" so the C construct where you can declare a method
with no parameters and then define it with a parameter list doesn't work.
Pretty much all the changes are around that.

I would like to get it to build as `/clr:pure` at some point. It doesn't
look too far off. The only annoying bit is that things like

```c
static const char rcsid[] = "$Id: am_map.c,v 1.4 1997/02/03 21:24:33 b1 Exp $";
```

don't compile with `/clr:pure`. The compiler has trouble generating the
static initialization support. I suspect it has to do with AppDomains.
I wish there was a way to tell the compiler to not worry about AppDomains.

Without `/clr:pure`, the compiler emits native code for some of the methods.
Most of DOOM is pure IL though. You can see in ILDASM. ILSpy can even decompile
some of it to C#.

To compile a CLR-hosted DOOM, run:

```bash
$ cl /clr /BC doom.c
```

`/BC` is an [undocumented switch](http://blog.airesoft.co.uk/2013/01/plug-in-to-cls-kitchen/)
that enables what I would call "C/CLI" or "Managed C" support
in the VC++ compiler. `cl` would reject compiling C with `/clr` without that.

------------------------------------------------------------------------
## ORIGINAL README FROM DOOM-CRT
------------------------------------------------------------------------


DOOM-CRT
========

One thing I’ve wanted to do for some time, is take the original source 
code release for DOOM, the one that only had linux support, and make my 
own port to windows, utilizing my single header libs and putting my crt 
filter on top of it. This is that thing.

Now, there are a lot of great windows ports, fixing bugs and adding 
features, and most of them would be a better choice if you want a DOOM 
which runs well on windows, or if you want some code to do serious work 
on top of. My port won’t try to compete with those efforts.

The main reason I’m doing it is for my own enjoyment. I’ve never really 
poked around in the DOOM code properly before, but always wanted to.

And also, I think it might be useful to make available a simple and 
minimalistic port. One which modifies the original code as little as 
possible, uses as few dependencies as possible (no large external 
frameworks) and which can be built without a complex build system.

It is not using any large frameworks, but relies on a few stb-style
single-file header-only libraries (which are all placed in the `libs_win32`
folder) which I have written myself - with the notable exception of
the TinySoundFont library, which is used as a back-end for music playback.

It’s not a particularly nice port, quick and dirty is more like it. 
Feel free to fork it and clean it up if you want to :-)


How to build
------------
To build, start a visual studio developer command prompt x86 ( it won’t 
work if you use the x64 command prompt) and run the command:

    cl doom.c

This should give you a runnable `doom.exe`. The wad file for the 
shareware version is in the repo as well, so you should be good to run
the exe straight away. Enjoy!

/Mattias Gustavsson

------------------------------------------------------------------------
## ORIGINAL README FROM ORIGINAL SOURCE CODE RELEASE FOLLOWS
------------------------------------------------------------------------

Here it is, at long last.  The DOOM source code is released for your
non-profit use.  You still need real DOOM data to work with this code.
If you don't actually own a real copy of one of the DOOMs, you should
still be able to find them at software stores.

Many thanks to Bernd Kreimeier for taking the time to clean up the
project and make sure that it actually works.  Projects tends to rot if
you leave it alone for a few years, and it takes effort for someone to
deal with it again.

The bad news:  this code only compiles and runs on linux.  We couldn't
release the dos code because of a copyrighted sound library we used
(wow, was that a mistake -- I write my own sound code now), and I
honestly don't even know what happened to the port that microsoft did
to windows.

Still, the code is quite portable, and it should be straightforward to
bring it up on just about any platform.

I wrote this code a long, long time ago, and there are plenty of things
that seem downright silly in retrospect (using polar coordinates for
clipping comes to mind), but overall it should still be a usefull base
to experiment and build on.

The basic rendering concept -- horizontal and vertical lines of constant
Z with fixed light shading per band was dead-on, but the implementation
could be improved dramatically from the original code if it were
revisited.  The way the rendering proceded from walls to floors to
sprites could be collapsed into a single front-to-back walk of the bsp
tree to collect information, then draw all the contents of a subsector
on the way back up the tree.  It requires treating floors and ceilings
as polygons, rather than just the gaps between walls, and it requires
clipping sprite billboards into subsector fragments, but it would be
The Right Thing.

The movement and line of sight checking against the lines is one of the
bigger misses that I look back on.  It is messy code that had some
failure cases, and there was a vastly simpler (and faster) solution
sitting in front of my face.  I used the BSP tree for rendering things,
but I didn't realize at the time that it could also be used for
environment testing.  Replacing the line of sight test with a bsp line
clip would be pretty easy.  Sweeping volumes for movement gets a bit
tougher, and touches on many of the challenges faced in quake / quake2
with edge bevels on polyhedrons.

Some project ideas:

Port it to your favorite operating system.

Add some rendering features -- transparency, look up / down, slopes,
etc.

Add some game features -- weapons, jumping, ducking, flying, etc.

Create a packet server based internet game.

Create a client / server based internet game.

Do a 3D accelerated version.  On modern hardware (fast pentium + 3DFX)
you probably wouldn't even need to be clever -- you could just draw the
entire level and get reasonable speed.  With a touch of effort, it should
easily lock at 60 fps (well, there are some issues with DOOM's 35 hz
timebase...).  The biggest issues would probably be the non-power of two
texture sizes and the walls composed of multiple textures.


I don't have a real good guess at how many people are going to be
playing with this, but if significant projects are undertaken, it would
be cool to see a level of community cooperation.  I know that most early
projects are going to be rough hacks done in isolation, but I would be
very pleased to see a coordinated 'net release of an improved, backwards
compatable version of DOOM on multiple platforms next year.

Have fun.

John Carmack
12-23-97
