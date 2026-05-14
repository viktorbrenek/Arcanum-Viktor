"""
bmp_to_art.py  —  Convert BMP images to TIG .art format for Arcanum CE.

Produces single-frame, single-rotation, 8-bit palette-indexed .art files
suitable for item inventory/ground art.

Usage:
    python bmp_to_art.py <input.bmp> <output.art>

Requires Pillow:
    pip install Pillow

After generating the .art files, place them in the game's data directory:
    <game_dir>/data/art/item/
e.g.:
    <game_dir>/data/art/item/orb_reforging_inven.art
    <game_dir>/data/art/item/orb_reforging_ground.art
    (repeat for ascension, cleansing, annulment)

The game's 'data' directory is created next to the arcanum*.dat files.
"""

import struct
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("ERROR: Pillow not installed. Run: pip install Pillow")


MAX_ROTATIONS = 8
MAX_PALETTES  = 4
TIG_ART_FLAG_SINGLE_ROTATION = 0x01  # TIG_ART_0x01


def quantize_to_8bit(img: Image.Image):
    """Return (palette_rgb_list, pixel_bytes) for an image quantized to <=256 colours."""
    if img.mode == "P":
        raw_palette = img.getpalette()  # flat R,G,B list, 256*3 entries
        palette_rgb = [(raw_palette[i*3], raw_palette[i*3+1], raw_palette[i*3+2])
                       for i in range(256)]
        pixels = bytes(img.tobytes())
        return palette_rgb, pixels

    # Convert to RGB then quantize.
    img_rgb = img.convert("RGB")
    img_q   = img_rgb.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
    raw_palette = img_q.getpalette()
    palette_rgb = [(raw_palette[i*3], raw_palette[i*3+1], raw_palette[i*3+2])
                   for i in range(256)]
    pixels = bytes(img_q.tobytes())
    return palette_rgb, pixels


def write_art(out_path: str, palette_rgb, pixels, width: int, height: int):
    """Write a TIG .art file with one frame, one rotation, one palette."""
    pixel_count = width * height
    assert len(pixels) == pixel_count, f"pixel mismatch: {len(pixels)} != {pixel_count}"

    # Build palette entries as uint32 RRGGBB (tig_color_index_of reads bits 16..23 as R, 8..15 as G, 0..7 as B).
    palette_u32 = []
    for r, g, b in palette_rgb:
        palette_u32.append((r << 16) | (g << 8) | b)

    with open(out_path, "wb") as f:
        # --- Header (132 bytes) ---
        f.write(struct.pack("<I", TIG_ART_FLAG_SINGLE_ROTATION))  # flags
        f.write(struct.pack("<i", 0))                              # fps
        f.write(struct.pack("<i", 8))                              # bpp

        # palette_tbl[4]: non-zero means palette present
        f.write(struct.pack("<iiii", 1, 0, 0, 0))

        f.write(struct.pack("<i", 0))   # action_frame
        f.write(struct.pack("<i", 1))   # num_frames

        # frames_tbl_ptrs[8] — disk offsets (ignored by loader)
        f.write(struct.pack("<" + "i" * MAX_ROTATIONS, *([0] * MAX_ROTATIONS)))

        # data_size[8] — pixel data size per rotation
        data_sizes = [pixel_count] + [0] * (MAX_ROTATIONS - 1)
        f.write(struct.pack("<" + "i" * MAX_ROTATIONS, *data_sizes))

        # pixels_tbl_ptrs[8] — ignored
        f.write(struct.pack("<" + "i" * MAX_ROTATIONS, *([0] * MAX_ROTATIONS)))

        # --- Palette 0 (1024 bytes) ---
        f.write(struct.pack("<" + "I" * 256, *palette_u32))

        # --- Frame data for rotation 0 (TigArtFileFrameData, 28 bytes) ---
        # width, height, data_size, hot_x, hot_y, offset_x, offset_y
        f.write(struct.pack("<iiiiiii", width, height, pixel_count, 0, 0, 0, 0))

        # --- Pixel data (top-to-bottom row order) ---
        # PIL returns pixels top-to-bottom for quantized images, which matches TIG.
        f.write(pixels)

    print(f"Wrote {out_path}  ({width}x{height}, {pixel_count} bytes of pixel data)")


def convert(in_path: str, out_path: str):
    img = Image.open(in_path)

    # Flip vertically if needed: PIL loads BMP bottom-up into top-down Image.
    # Image.open already handles the BMP vertical flip, so img is top-to-bottom.

    palette_rgb, pixels = quantize_to_8bit(img)
    write_art(out_path, palette_rgb, pixels, img.width, img.height)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python bmp_to_art.py <input.bmp> <output.art>")
        sys.exit(1)
    convert(sys.argv[1], sys.argv[2])
