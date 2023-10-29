# OBS PNG Tuber Plugin

This is a fork of [ashmanix/obs-image-reaction](https://github.com/ashmanix/obs-image-reaction) trying to recreate blinking like Veadotube mini does.
With this lightweight approach, Linux users have an easier time being a PNGTuber.

You need 4 images of your character to make use of this plugin:

- silent (eyes open)
- talking (eyes open)
- silent (eyes closed)
- talking (eyes closed)

Currently it will blink at a fixed interval and it will blink for a fixed time.
Randomized blinking intervals are planned for the near future.

Please reach out to me if you want to help build a proper release from this.
It is currently very untested and just for my usecase.

## Building for Linux & Installing

These steps assume that you want to install the plugin in your home folder

1. clone repo and cd to repo directory

2. create a build directory, generate Makefile and compile the project
`mkdir build && cd build && cmake .. && make`

3. copy plugin to plugin directory:
`cp -r rundir/RelWithDebInfo/data/obs-plugins/obs-pngtuber ~/.config/obs-studio/plugins/`

4. create directoy for executable:
`mkdir -p ~/.config/obs-studio/plugins/obs-pngtuber/bin/64bit`

5. copy executable to plugin directory:
`cp obs-pngtuber.so ~/.config/obs-studio/plugins/obs-pngtuber/bin/64bit/`

## Windows

Instructions from [scaledteams version of the plugin](https://github.com/scaledteam/obs-image-reaction) might work for you.
You are better off using Veadotube directly though.

## MacOS

ashmanix's fork introduced MacOS support.
I do not own any MacOS devices though and I don't know where to start building for MacOS.
Please step up and contact me if you know how this works.
