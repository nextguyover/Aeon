import os
import subprocess
import argparse

from PIL import Image, ImageEnhance, ImageOps

from pillow_heif import register_heif_opener  # Support for HEIC images
register_heif_opener()

def scale(image: Image, target_width=400, target_height=480) -> Image:
    """
    Scale an image by resizing and centrally cropping it to target dimensions.
    
    Steps:
    1. Determine new dimensions while keeping the image proportional.
    2. Resize the image using high-quality resampling.
    3. Crop the resized image centrally to exact target size.
    """
    width, height = image.size

    if height / width < target_height / target_width:
        print('too wide: cropping')
        new_height = target_height
        new_width = int(width * new_height / height)
    else:
        print('too tall: cropping')
        new_width = target_width
        new_height = int(height * new_width / width)

    print(f"Original (h, w): ({height}, {width}) -> Scaled (h, w): ({new_height}, {new_width})")

    # Resize image with high quality resampling
    ANTIALIAS = Image.Resampling.LANCZOS
    img = image.resize((new_width, new_height), ANTIALIAS)

    # Calculate crop region for central crop
    half_width_delta = (new_width - target_width) // 2
    half_height_delta = (new_height - target_height) // 2
    img = img.crop((half_width_delta, half_height_delta,
                    half_width_delta + target_width, half_height_delta + target_height))
    return img


def ensure_directory(path: str) -> None:
    """
    Ensure that the directory at 'path' exists.
    
    If it does not exist, create the directory.
    """
    if not os.path.exists(path):
        os.makedirs(path)


def create_custom_palette() -> Image:
    """
    Create a custom palette for the 7-color e-ink display.
    
    Steps:
    1. Start with a new palette image.
    2. Define the primary 7 colors.
    3. Fill the rest of the 256-color palette with a dummy color (black).
    """
    pal_image = Image.new("P", (1, 1))
    pal_image.putpalette(
        (
            0, 0, 0, 
            255, 255, 255, 
            0, 255, 0, 
            0, 0, 255, 
            255, 0, 0, 
            255, 255, 0, 
            255, 125, 0
        ) + (0, 0, 0) * 249)
    return pal_image


def process_image(input_path: str, output_path: str, palette: Image) -> None:
    """
    Process a single image:
    
    1. Open the image from the input path.
    2. Apply EXIF orientation corrections.
    3. Adjust image size (reduce width by half) and scale to target dimensions.
    4. Enhance colors to improve dithering.
    5. Convert the image using the custom palette.
    6. Save the processed (dithered) image to the output path.
    """
    original_image = Image.open(input_path)

    # Correct orientation based on EXIF data
    transposed_image = ImageOps.exif_transpose(original_image)

    # Adjust image size: reduce width by half (as an example adjustment)
    width, height = transposed_image.size
    adjusted_image = transposed_image.resize((width // 2, height), Image.Resampling.LANCZOS)

    # Scale image to target dimensions
    scaled_image = scale(adjusted_image)

    # Enhance image color for a better dithering result
    enhanced_image = ImageEnhance.Color(scaled_image).enhance(3)

    # Convert image to use the custom 7-color palette with dithering
    dithered_image = enhanced_image.convert("RGB").quantize(palette=palette)
    dithered_image.save(output_path)
    print(f"Saved dithered image to {output_path}")


def process_images(input_dir: str, intermediary_dir: str) -> None:
    """
    Process all supported images found in the input directory:
    
    1. Ensure the intermediary directory exists.
    2. Create the custom palette to be reused for all images.
    3. Iterate through each image file in the input directory.
    4. For each supported image format (.jpg, .jpeg, .png, .heic),
       process the image and save a dithered version in the intermediary directory.
    """
    ensure_directory(intermediary_dir)
    
    palette = create_custom_palette()
    
    for filename in os.listdir(input_dir):
        if filename.lower().endswith(('.jpg', '.jpeg', '.png', '.heic')):
            print(f'Processing {filename}...')
            input_path = os.path.join(input_dir, filename)
            
            basename = os.path.splitext(filename)[0]
            output_filename = f"{basename}_dithered.bmp"
            output_path = os.path.join(intermediary_dir, output_filename)
            
            process_image(input_path, output_path, palette)


def run_slic_conv(input_dir: str, output_dir: str) -> None:
    """
    Convert intermediary images to slic format:

    1. Ensure the output directory exists.
    2. For each image file in the input directory, call the external command 'slic_conv'
       to perform the conversion.
    3. Name the output files with sequential numbering.
    """
    ensure_directory(output_dir)

    files = os.listdir(input_dir)

    # sort files by name
    files.sort()

    for index, file in enumerate(files):
        infile = os.path.join(input_dir, file)
        outfile = os.path.join(output_dir, f"{index}.slc")
        
        print(f"Running slic_conv: {infile} -> {outfile}")
        subprocess.run(['slic_conv', infile, outfile], check=True)


def main():
    """
    Convert images to the SLIC format for the e-ink display.
    """
    parser = argparse.ArgumentParser(description="Convert images for the e-ink display")
    parser.add_argument("input_dir", help="Input images directory")
    args = parser.parse_args()

    input_img_dir = args.input_dir
    intermediary_dir = './img_intermediary'
    output_dir = './img_out'

    # clear intermediary and output directories
    if os.path.exists(intermediary_dir):
        os.system(f'rm -r {intermediary_dir}')

    if os.path.exists(output_dir):
        os.system(f'rm -r {output_dir}')

    process_images(input_img_dir, intermediary_dir)
    run_slic_conv(intermediary_dir, output_dir)


if __name__ == "__main__":
    main()