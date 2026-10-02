# vfs.pdf

A Kodi VFS add-on that opens a PDF as a folder holding a picture of each page:

    pdf://<the PDF's path, URL encoded>/0001.jpg

so anything in Kodi that shows pictures can show a PDF's pages. It was written
for game manuals, which [script.game.manuals](https://github.com/sunlollyking/script.game.manuals)
shows with it, but a PDF in the Pictures section opens the same way.

Pages are drawn by [Poppler](https://poppler.freedesktop.org) onto white, as
JPEGs whose longer side is 2560 pixels: enough to zoom in on small print while
staying inside the texture size most devices allow.

## Build

Like any Kodi binary add-on. Poppler's C++ frontend is needed; built
statically, its own dependencies (FreeType, fontconfig, libjpeg, libpng,
lcms2, zlib) are found through pkg-config.

    cmake -DKODI_INCLUDE_DIR=<kodi>/xbmc/addons/kodi-dev-kit/include/kodi \
          -DBUILD_TESTING=ON <this folder>
    make && ctest

The test renders a PDF it builds itself and needs no Kodi.

## Licence

GPL-2.0-or-later, as Poppler.
