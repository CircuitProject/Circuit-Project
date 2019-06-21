
Debian
====================
This directory contains files used to package circuitd/circuit-qt
for Debian-based Linux systems. If you compile circuitd/circuit-qt yourself, there are some useful files here.

## circuit: URI support ##


circuit-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install circuit-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your circuit-qt binary to `/usr/bin`
and the `../../share/pixmaps/circuit128.png` to `/usr/share/pixmaps`

circuit-qt.protocol (KDE)

