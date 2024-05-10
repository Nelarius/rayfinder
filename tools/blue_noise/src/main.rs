use image::GenericImageView;
use std::fs::File;
use std::io::Write;

fn main() {
    // Load the image.
    let img = image::open("128_128_LDR_RG01_0.png").unwrap();
    let (width, height) = img.dimensions();

    // Prepare the output file.
    let mut header = File::create("blue_noise.h").unwrap();

    write!(header, "#pragma once\n\n").unwrap();
    write!(header, "#include <stddef.h>\n").unwrap();
    write!(header, "#include <stdint.h>\n\n").unwrap();
    write!(header, "#ifdef __cplusplus\n    extern \"C\" {{\n#endif\n").unwrap();
    // Write the array declaration to the file.
    write!(
        header,
        "// Array contains consecutive R, G values. Pixels are indexed from the top-left.\n"
    )
    .unwrap();
    write!(
        header,
        "extern const uint8_t blueNoiseValues[{}];\n\n",
        2 * height * width
    )
    .unwrap();
    write!(header, "extern const size_t blueNoiseWidth;\n").unwrap();
    write!(header, "extern const size_t blueNoiseHeight;\n").unwrap();
    write!(header, "#ifdef __cplusplus\n    }}\n#endif\n").unwrap();

    let mut source = File::create("blue_noise.c").unwrap();
    write!(source, "#include \"blue_noise.h\"\n\n").unwrap();
    write!(
        source,
        "const uint8_t blueNoiseValues[{}] = {{\n",
        2 * height * width
    )
    .unwrap();
    // Iterate over the pixels of the image.
    for y in 0..height {
        for x in 0..width {
            // Get the pixel and the red channel.
            let pixel = img.get_pixel(x, y);
            let r = pixel[0];
            let g = pixel[1];

            // Write the red channel value to the file.
            write!(source, "{}, {}, ", r, g).unwrap();
        }
    }

    // Write the closing brace to the file.
    write!(source, "}};\n").unwrap();
    write!(source, "const size_t blueNoiseWidth = {};\n", width).unwrap();
    write!(source, "const size_t blueNoiseHeight = {};\n", height).unwrap();
}
