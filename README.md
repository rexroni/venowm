# Venowm

A Ratpoison-inspired Wayland compositor.

Pronounced like "venom", and definitely *not* pronounced like "vee-no window manager".

## Project Goals:

- Recreate a [ratpoison](https://savannah.nongnu.org/projects/ratpoison)-like environment in a post-Xorg world.

- Native (as in C-code-not-Perl-script) support for workspaces, inspired by the [jcs/ratpoison](https://github.com/jcs/ratpoison) fork.

- Support hot-plugging of displays.  It is embarrassing when you have to explain to your Mac-using coworkers that you need to close all of your windows and restart your window manager every time you unplug a display from your laptop.

- (Long-term) Eventually support Nvidia proprietary driver.  Because work laptops running CUDA deserve a great window manager, too.

## Project Status:

- Not yet usable, not even by the developer himself.

## Build and Run Instructions:

1. Install dependencies:

    - `weston` (for the wetson-terminal and weston-info)

    - `wlroots >= 0.6.0`

    - `libwayland` and `libwayland-server`

    - `libxkbcommon`

    - Possibly some other packages.

1. Run `make`.

1. Execute with `./venowm`.

1. Play around in your shiny new venowm environment:
    - Launch more windows with "ctrl-enter" (currently hard-coded to launch `weston-terminal`)
    - Split the screen with "ctrl-minus" or "control-backslash".
    - Move between frames with "ctrl-h/j/k/l".
    - Drag windows around with "ctrl-shift-h/j/k/l".
    - Close frames with "ctrl-y".
    - Cycle through hidden windows with "ctrl-space".
    - Press "ctrl-q" to quit.

## License

The file `khash.h` is from attractivechaos's [klib](https://github.com/attractivechaos/klib) project.

All other source files in Venowm represent original work for the Venowm project are in the public domain, under the conditions of the [unlicense](https://unlicense.org/).
