# JPEG X-Ray — Security and Correctness Review

This review is based on the current `jpeg-xray.c` implementation in this repository. It is a static code review; no claim is made that all malformed-input cases have been fixed or that the program has passed a comprehensive test suite.

## Findings

### Critical — EXIF/TIFF reads are not bounded by the APP1 segment

Functions including `exif_ayristir`, `alt_ifd_ayristir`, `gps_ayristir`, and `rasyonel_oku` calculate pointers from offsets read from the input file. The code does not consistently verify that the offset and the complete requested field lie inside the EXIF/TIFF payload. IFD entry counts are also trusted without checking that all entries fit in the segment.

**Impact:** malformed JPEG metadata can trigger out-of-bounds reads and crashes; depending on the path, this can become a memory-safety issue.

**Fix:** pass the buffer length through every parser, use checked offset/length arithmetic, validate TIFF byte order and magic, validate IFD tables before iteration, and validate each field's type/count/size before dereferencing.

### Critical — Unbounded EXIF string copies

The EXIF parser uses `strcpy` for camera make/model, dates, software, and copyright. Input strings are not first proven to be NUL-terminated within the APP1 payload, and their lengths are not checked against destination capacities.

**Impact:** buffer overflow or out-of-bounds read on malformed or deliberately crafted metadata.

**Fix:** validate the EXIF ASCII count and source range, then copy at most destination capacity minus one and append a NUL byte.

### High — XMP search assumes NUL termination

`xmp_ayristir` calls `strstr` on an APP1 payload. JPEG segment payloads are length-delimited byte arrays, not C strings; a NUL terminator is not guaranteed.

**Impact:** search can read beyond the allocated segment buffer.

**Fix:** use a bounded byte-search routine that receives the payload length, or create a checked NUL-terminated copy.

### High — JPEG parsing does not handle SOS/entropy-coded data correctly

After Start Of Scan (SOS), JPEG image data is entropy-coded. The current loop continues treating every `0xFF` sequence as a normal segment marker and reads a two-byte segment length after markers that may be stuffed bytes or restart markers.

**Impact:** parsing can lose synchronization, stop early, or interpret compressed image bytes as segment lengths.

**Fix:** either stop metadata scanning at SOS (sufficient for the metadata currently targeted before the scan), or implement the JPEG entropy-data rules, including `FF 00` byte stuffing and restart markers.

### High — Segment and file I/O results are not fully validated

The main loop does not robustly handle EOF while reading marker/length bytes, does not check whether `malloc` succeeded, and does not verify that `fread` read the full segment payload. Marker-specific payload lengths are not always checked before parser functions access fixed offsets.

**Impact:** truncated or malformed files can cause invalid reads, incorrect reports, or crashes.

**Fix:** validate each byte read, reject incomplete lengths, check allocation, require the exact payload byte count, and validate minimum payload sizes before dispatch.

### Medium — DQT quality calculation is not a reliable quality metric

The current calculation sums 64 bytes beginning at offset 1 and maps the average to a 1–100 value. It does not account for multiple tables, 16-bit quantization precision, table selectors, or a validated encoder-specific quality mapping.

**Impact:** the displayed number can be misleading and should not be interpreted as the JPEG encoder's original quality setting.

**Fix:** parse every DQT table according to its precision and identifier. If estimating quality, document the assumptions and test against known encoders; otherwise report the quantization tables without a quality number.

### Medium — EXIF field types and counts are incompletely validated

Several fields are read using assumptions about their representation. For example, ISO is read as a 16-bit value, while the parser does not consistently validate the TIFF type and count for every tag.

**Impact:** valid files using other allowed representations may be misread; malformed entries can be interpreted incorrectly.

**Fix:** validate each tag's TIFF type and count, and support the relevant SHORT/LONG/RATIONAL/ASCII representations explicitly.

### Medium — GPS presence detection can miss valid coordinates

The code marks GPS as present only if the latitude or longitude degree component is nonzero. A valid coordinate on the equator or prime meridian can have a zero degree component.

**Impact:** valid GPS metadata may be reported as unavailable.

**Fix:** track whether the required GPS tags were successfully parsed, independently of their numeric values, and validate coordinate ranges and reference tags.

### Low — Thumbnail indicator overstates what was checked

The code sets the thumbnail flag when it sees tag `0x0201` (JPEGInterchangeFormat offset). It does not validate the corresponding length tag or the thumbnail bytes.

**Impact:** the report may say a thumbnail is present when the thumbnail data is incomplete or invalid.

**Fix:** require and validate both offset and length, then verify the referenced byte range.

## Build and test checklist

The repository should not be considered hardened until these tests pass:

- [ ] Compile with GCC and Clang using `-Wall -Wextra -Wpedantic`.
- [ ] Run with AddressSanitizer and UndefinedBehaviorSanitizer.
- [ ] Valid JPEG with no metadata.
- [ ] Valid JPEG with JFIF and SOF0/SOF2.
- [ ] EXIF in both little-endian and big-endian TIFF byte order.
- [ ] EXIF strings at and beyond destination-buffer limits.
- [ ] EXIF offsets outside APP1 and oversized IFD entry counts.
- [ ] Truncated JPEG marker, segment length, and segment payload.
- [ ] JPEG with SOS, stuffed `FF 00` bytes, and restart markers.
- [ ] GPS coordinates on the equator and prime meridian.
- [ ] DQT tables with 8-bit and 16-bit precision.

## Scope

This document records issues visible in the current source and proposes remediation. It is not a penetration test, formal verification, or a claim that every possible defect has been found.
