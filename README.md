# NppFTP
Plugin for Notepad++ allowing FTP, FTPS, FTPES and SFTP communications

This fork is currently focused on security hardening, reproducible Windows
builds, and a simpler PSPad-style remote browser for day-to-day FTP work.

- Further information could be found at [NppFTP](http://ashkulz.github.io/NppFTP/)
- For a video tutorial on installation and first steps see [#305](https://github.com/ashkulz/NppFTP/issues/305)

- Build HowTo [BUILDING.md](https://github.com/ashkulz/NppFTP/blob/master/BUILDING.md)

Maintained Fork Notes
---------------------

Recent work in this branch:

- Added `build.bat` and `build_scripts.ps1` for the current Windows build path.
- Recorded third-party source URLs, licenses, and SHA256 checksums in
  `third_party_sources.md`.
- Hardened FTPS hostname/certificate handling, FTP PASV parsing, cache path
  handling, SFTP path composition, DPAPI password storage, CI permissions, and
  FTP response/listing caps.
- Reworked the remote browser into a flat current-directory view with current
  path, quick search, editable recent-directory combo, folder/file icons,
  metadata columns, header drag/drop, busy cursor feedback, and safer typed path
  handling.

Build quick start:

```bat
build.bat
```

The package is written to `_build\NppFTP-0.30.22-win64.zip`; the plugin DLL is
written to `_build\Release\NppFTP.dll`.

Project notes:

- `history.md` records important fixes, build hashes, and decisions.
- `todo.md` tracks remaining work.
- `docs/superpowers/plans/` keeps the task plans used for larger slices.

Build Status
------------

[![Appveyor build status](https://ci.appveyor.com/api/projects/status/github/ashkulz/nppftp?branch=master&svg=true)](https://ci.appveyor.com/project/ashkulz/nppftp)
[![GitHub release](https://img.shields.io/github/release/ashkulz/NppFTP.svg)](https://github.com/ashkulz/NppFTP/releases)
