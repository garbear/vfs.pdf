# vfs.pdf

A Kodi VFS add-on that opens a PDF as a folder holding a picture of each page:

    pdf://<the PDF's path, URL encoded>/0001.jpg

so anything in Kodi that shows pictures can show a PDF's pages. It was written
for game manuals, which [script.game.manuals](https://github.com/sunlollyking/script.game.manuals)
shows with it.

Only the `pdf://` protocol is registered, not the `.pdf` extension. Kodi adds
an add-on's extensions to its video, music and picture lists, so claiming
`.pdf` would put every PDF into every media listing; an add-on that wants a
PDF's pages builds the `pdf://` path itself.

Pages are drawn by [Poppler](https://poppler.freedesktop.org) onto white, as
JPEGs whose longer side is 2560 pixels: enough to zoom in on small print while
staying inside the texture size most devices allow.

## Build

Like any Kodi binary add-on, through Kodi's `cmake/addons`; `depends/common`
builds FreeType, OpenJPEG and Poppler's C++ frontend statically, with libjpeg
and (on Linux) fontconfig from the system. A system Poppler is used instead if
one is found, its own dependencies coming from pkg-config.

Poppler must be built with OpenJPEG. Scanned manuals mostly keep their pages
as JPEG 2000, and without it those pages are drawn blank.

The page renderer's test needs Poppler and libjpeg but not Kodi:

    cmake -S tests -B build-tests && cmake --build build-tests
    ctest --test-dir build-tests

It renders a PDF it builds itself. Linux, including LibreELEC, is what has been
built and run; the other platforms go through the same `depends` but have not
been tried.

## Licence

GPL-2.0-or-later, as Poppler.
