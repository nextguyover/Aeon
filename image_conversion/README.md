Run conversion script as instructed in top level `README.md`.

The script will generate the following folders:
- `img_intermediary`: contains the cropped and processed images in BMP format. This is how the final images will appear on the e-ink display.
- `img_packed`: contains the packed images where each byte represents two consecutive 4-bit pixels (this is the format that data is sent to e-ink display). The colours of images in this folder are not visually representative.
- `img_out`: contains the final SLIC converted images in the format compatible with Aeon firmware. These should all be copied to the correct directory on the SD card without renaming or omitting any files.