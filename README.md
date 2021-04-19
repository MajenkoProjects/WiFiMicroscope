WiFi Microscope Viewer
======================

This is a simple viewer for cheap WiFi microscope cameras. This only supports 1280x720
resolution at the moment.

Tested with the [Dodocool Microscope](https://www.amazon.co.uk/Microscope-Wireless-Magnifying-Endoscope-Inspection/dp/B08NP37B7X)

It requires SDL2 and SDL2_image to operate:

```bash
$ sudo apt install libsdl2-dev libsdl2-image-dev
```

To compile just type `make`


Interaction
===========

There are a few keys bound to specific operations:

* q : Quit
* m : Toggle monochrome mode
* i : Toggle invert colour mode
* [ : Decrease brightness
* ] : Increase brightness
* ' : Decrease contrast
* # : Increase contrast
* r : Reset brightness and contrast to default
* s : Save a JPEG snapshot to the current directory named `microscope_<time>.jpg`
