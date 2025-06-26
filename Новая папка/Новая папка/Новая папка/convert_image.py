from PIL import Image
import argparse
import os
import sys

def convert_image_to_c_array(image_path, output_path, array_name="image_data"):
    try:
        # Открываем изображение
        img = Image.open(image_path)
        img = img.convert('RGB')
        img = img.resize((128, 160))
        
        # Создаем выходной файл
        with open(output_path, 'w') as f:
            f.write("#pragma once\n")
            f.write("#include <stdint.h>\n\n")
            f.write(f"const uint16_t {array_name}[{128 * 160}] = {{\n")
            
            # Проходим по каждому пикселю
            pixels = img.load()
            for y in range(160):
                for x in range(128):
                    r, g, b = pixels[x, y]
                    
                    # Варианты конвертации (попробуйте оба, если цвета неправильные)
                    # Вариант 1: Стандартный порядок RGB -> RGB565
                    rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                    
                    # Вариант 2: Поменять местами R и B (BGR)
                    #rgb565 = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3)
                    
                    f.write(f"0x{rgb565:04X}, ")
                
                f.write("\n")
            
            f.write("};\n")
        
        print(f"Successfully converted {image_path} to {output_path}")
        print(f"Array name: {array_name}, Size: 128x160 = {128*160} pixels")
        return True
        
    except Exception as e:
        print(f"Error: {str(e)}")
        return False

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Convert image to C array for ST7735 display')
    parser.add_argument('input', help='Input image file path')
    parser.add_argument('output', help='Output .h file path')
    parser.add_argument('--name', default='image_data', help='Name for C array')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.input):
        print(f"Error: Input file {args.input} does not exist!")
        sys.exit(1)
    
    if convert_image_to_c_array(args.input, args.output, args.name):
        print("Conversion successful!")
    else:
        print("Conversion failed!")
        sys.exit(1)