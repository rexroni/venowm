# Venowm

A Ratpoison-inspired Wayland compositor.

Pronounced like "venom", and definitely *not* pronounced like "vee-no window manager".

## Project Goals:

- Recreate a [ratpoison](https://savannah.nongnu.org/projects/ratpoison)-like environment in a post-Xorg world.

- Native (as in C-code-not-Perl-script) support for workspaces, inspired by the [jcs/ratpoison](https://github.com/jcs/ratpoison) fork.

- Support hot-plugging of displays.  It is embarrassing when you have to explain to your Mac-using coworkers that you need to close all of your windows every time you unplug a display from your laptop.

- (Long-term) Eventually support Nvidia proprietary driver.  Because work laptops running CUDA deserve a great window manager, too.

## Project Status:

- Not yet usable, not even by the developer himself.

## Build and Run Instructions:

1. Install dependencies:

    - [libswc](https://github.com/michaelforney/swc)

    - `libwayland` and `libwayland-server`.

    - Possibly some other packages.

1. Run `make`

1. Execute from a console (not X) with `swc-launch ./venowm`.

1. Press "ctrl-q" to quit.
