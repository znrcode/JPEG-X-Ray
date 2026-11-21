# JPEG X-Ray

JPEG X-Ray is a command-line program written in C that inspects JPEG marker segments and reports selected image and metadata fields. The project is intended as a hands-on exercise in binary file parsing and the JPEG/EXIF data layout.

> **Project status:** experimental. The current parser is a learning prototype, not a hardened parser for untrusted or malformed files. See [Security and correctness review](SECURITY_REVIEW.md) before using it on files from unknown sources.

## Current capabilities

- Checks the JPEG Start Of Image (SOI) signature.
- Reads image dimensions from SOF0 and SOF2 segments.
- Reads JFIF density values.
- Parses selected EXIF fields:
  - Camera make and model
  - Capture and modification dates
  - Software and copyright
  - ISO, exposure time, F-number, and focal length
  - GPS latitude and longitude
  - X/Y resolution and resolution unit
  - Thumbnail offset presence indicator
- Detects selected metadata:
  - XMP marker text and a specific `Label="Red"` value
  - ICC profile marker
  - Photoshop APP13 marker
  - JPEG COM comment
- Calculates a rough value from the first 8-bit DQT table encountered.

The implementation does **not** currently support every JPEG marker, every EXIF field, all TIFF data types, or complete ICC/XMP profile reconstruction.

## Requirements

- A C compiler (GCC or Clang)
- Standard C library

## Build

Linux, macOS, or MinGW-w64:

```bash
gcc -std=c11 -Wall -Wextra -O2 jpeg-xray.c -o jpeg-xray
```

Windows executable:

```bash
gcc -std=c11 -Wall -Wextra -O2 jpeg-xray.c -o jpeg-xray.exe
```

## Run

```bash
./jpeg-xray path/to/image.jpg
```

On Windows:

```powershell
.\jpeg-xray.exe path\to\image.jpg
```

The program prints a report to the terminal. Metadata that is not found is displayed as unavailable.

## Example report layout

```text
File: image.jpg

[Basic JPEG Info]
Width Value        : ...
Height Value       : ...

[EXIF]
Camera Make        : ...
Camera Model       : ...
Date Taken         : ...
ISO Value          : ...

[GPS]
Latitude           : ...
Longitude          : ...
```

Values depend on the metadata present in the input image. This is an illustration of the report layout, not a guaranteed output for every JPEG.

## Repository layout

```text
JPEG-X-Ray/
├── jpeg-xray.c
├── README.md
└── SECURITY_REVIEW.md
```

## Known limitations

- EXIF/TIFF offsets and IFD entry counts are not consistently checked against the APP1 buffer length. Malformed metadata can cause out-of-bounds reads.
- Several EXIF strings are copied with unbounded `strcpy`; a crafted or malformed file can overflow destination buffers.
- XMP is searched with `strstr` even though an APP1 segment is a length-delimited byte buffer and is not guaranteed to end with a NUL byte.
- JPEG segment parsing does not correctly transition at SOS (Start Of Scan). Entropy-coded image data can be mistaken for marker/segment data.
- Segment reads, allocation failures, truncated files, and some marker-specific minimum lengths are not fully validated.
- DQT quality output is only a heuristic. The current calculation does not represent a standardized or reliable encoder quality setting.
- GPS presence detection does not correctly distinguish every valid coordinate, including coordinates whose degree component is zero.
- The thumbnail check only detects an EXIF offset tag; it does not validate or extract a complete thumbnail image.

For the complete findings and recommended fixes, see [SECURITY_REVIEW.md](SECURITY_REVIEW.md).

## Development priorities

1. Add length-aware bounds checks to every JPEG and TIFF/EXIF read.
2. Replace unbounded string operations and make XMP scanning length-aware.
3. Correct JPEG marker handling, especially SOS and entropy-coded data.
4. Validate allocation and file-read results.
5. Replace the DQT heuristic with a clearly labelled, tested estimate or remove it.
6. Add a regression test corpus: valid JPEGs, EXIF little-/big-endian samples, truncated files, malformed offsets, and files without metadata.

## License

No open-source license has been added yet. Until a license is added, the source remains under the copyright owner's default rights.
