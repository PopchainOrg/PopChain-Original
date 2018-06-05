
Debian
====================
This directory contains files used to package popd/pop-qt
for Debian-based Linux systems. If you compile popd/pop-qt yourself, there are some useful files here.

## pop: URI support ##


pop-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install pop-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your pop-qt binary to `/usr/bin`
and the `../../share/pixmaps/pop128.png` to `/usr/share/pixmaps`

pop-qt.protocol (KDE)

