===============================================================================
Image Eye - The lean, mean and clean image viewer
===============================================================================
Copyright 2015 by Markus Dimdal and FMJ-Software.

-------------------------------------------------------------------------------
Folder contents:

Image Eye.*				Visual C++ 2015 workspace for building Image Eye.

ReadMe.txt				This file.
To-do.txt				List of ideas for future versions.
Translations.txt		Instructions on how to create a GUI translation.
gpl.txt					License terms (GPLv3).

Setup *.iss				Inno Setup installation scripts

Hlp\					< Source for help file (HtmlHelp format) >

Out\					< Compiled binaries + language files et c >

Res\					< Resource files, icons et c >

Src\					< Source code >

-------------------------------------------------------------------------------

To build Image Eye you need to 4 things placed under a common root folder <x>:

<x>\Image Eye			< This repository: https://github.com/FMJ-Software/Image-Eye >
<x>\ieC++				< Image Engine C++ library: https://github.com/FMJ-Software/ieCpp >
<x>\md					< Misc. helper code: https://github.com/FMJ-Software/md >
<x>\zlib				< zlib codec: http://www.zlib.net/ >

The first three all need to be of the latest version to compile together.

IMPORTANT NOTE!	

If you make any changes *what-so-ever* to the code, then you MUST change the 
PROGRAMNAME and PROGCOMPANY defines in "Image Eye.h" to names of your own!
This is required to avoid conflicts in the Windows registry as well as
shared in-memory mappings of the program settings.

You may NOT use the names "Image Eye" and "FMJ-Software" for any unapproved releases.

-end-of-file-
-------------------------------------------------------------------------------
