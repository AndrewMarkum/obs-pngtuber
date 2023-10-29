# OBS PNG Tuber Plugin

This is a fork of [ashmanix/obs-image-reaction](https://github.com/ashmanix/obs-image-reaction) trying to recreate blinking like Veadotube mini does.

## Linux installation

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
