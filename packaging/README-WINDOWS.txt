SkeletonHive for Windows (portable)
===================================

This folder contains a portable Release build of SkeletonHive. No installer is
required — run SkeletonHive.exe directly.

Source code (AGPLv3)
--------------------
https://github.com/mark-speer/SkeletonHive

This binary corresponds to the public repository above. LICENSE and NOTICE are
included in this package.

Quick start
-----------
1. Double-click SkeletonHive.exe (or run it from a terminal).
2. Open Preferences → Audio and select your device type (ASIO recommended for
   low latency on Windows).
3. VST3 plugins are scanned from:
   C:\Program Files\Common Files\VST3

Notes
-----
- Settings and user data are stored under %AppData%\SkeletonHive\
- Default projects folder: Documents\SkeletonHive
- Plugin sandbox (crash isolation) can be enabled in Preferences → Devices
- This is a single-file build; no extra DLLs need to sit beside the exe

System requirements
-------------------
- 64-bit Windows 10 or later
- Visual C++ redistributable is not required (static CRT)
