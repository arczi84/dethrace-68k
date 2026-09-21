# MiniGL v12 client SDK snapshot

This is the small client interface used by this Dethrace build, not the
MiniGL renderer or minigl.library. Keep the headers and import archive together.
The dispatch table checks ABI version and structure size at library open.

The snapshot comes from the PiStorm3D MiniGL v12 client SDK. Original MiniGL
headers retain their copyright notices; see Licence.txt. SDI compatibility
headers in ../showcase/include retain their public-domain notices.

The 840-byte lib/libminigl_dispatch.a is the import archive used by the tested
R7–R10 game builds. Its client source is included in client/. To rebuild it with
an Amiga GCC toolchain, run make in this directory (override CROSS if needed).
The normal game build uses the supplied archive and does not rebuild it.
