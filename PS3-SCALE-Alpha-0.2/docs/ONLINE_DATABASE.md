# Online enhancement catalog design

The online catalog will not execute downloaded files or trust arbitrary URLs.

Each catalog item must include:
- title_id
- region / product code
- supported game update version
- platform (PS3 or PS2)
- patch kind (EBOOT patch, PS2 netemu CONFIG, texture package, etc.)
- SHA-256
- file size
- source URL and license/author
- tested hardware/firmware
- reversible backup instructions
- test status and known issues

The app should download only catalogued files, verify SHA-256 before use, store downloads in its
own data directory, and require explicit confirmation before applying anything. A PSN account is
not required for a normal HTTPS catalog.
