#pragma once

// All files the app writes live under one folder on the SD card instead of the
// root. The directory is created at startup by ensureAppDataDir() in main.cpp.
#define APPDATA_DIR      "sdmc:/switch/NX-torrent-player"
#define APPDATA_LOG      APPDATA_DIR "/nx-torrent-player.log"
#define APPDATA_CACHE    APPDATA_DIR "/cache.bin"
// Folder the user drops .torrent files into (scanned for the main menu list).
#define APPDATA_TORRENTS APPDATA_DIR "/torrents"
// One magnet link (or bare info-hash) per line; listed alongside the .torrents.
// Also where "Add magnet" appends what the on-screen keyboard captures.
#define APPDATA_MAGNETS  APPDATA_TORRENTS "/magnet.txt"
// App-managed cache of a magnet's video files, so its file picker works without
// re-fetching metadata: "<hash>\t<index>\t<size>\t<name>" lines (index -1 = a
// resolved magnet with no video inside). Written by the background resolver.
#define APPDATA_MAGNET_FILES APPDATA_DIR "/magnet-files.tsv"
// Cached Stremio artwork, one .jpg per library item. Posters never change, so
// entries are kept forever and a hit never touches the network.
#define APPDATA_POSTERS  APPDATA_DIR "/posters"
