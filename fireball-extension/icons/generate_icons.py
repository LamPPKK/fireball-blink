import struct
import zlib
import os

def create_png(width, height, fill_color, border_color):
    raw_data = bytearray()
    
    for y in range(height):
        raw_data.append(0) # Filter byte 0 (None)
        for x in range(width):
            # Rounded corner calculation
            radius = width // 4
            dx = min(x, width - 1 - x)
            dy = min(y, height - 1 - y)
            
            is_inside_corner = True
            if dx < radius and dy < radius:
                dist_sq = (radius - dx) ** 2 + (radius - dy) ** 2
                if dist_sq > radius ** 2:
                    is_inside_corner = False
                    
            is_border = (x == 0 or x == width - 1 or y == 0 or y == height - 1 or
                         dx < 2 or dy < 2)
            
            if not is_inside_corner:
                raw_data.extend([0, 0, 0, 0]) # Transparent
            elif is_border:
                raw_data.extend(border_color)
            else:
                # Gradient flame fill (Orange to Lime)
                ratio = (x + y) / (width + height)
                r = int(fill_color[0] * (1 - ratio) + 184 * ratio)
                g = int(fill_color[1] * (1 - ratio) + 255 * ratio)
                b = int(fill_color[2] * (1 - ratio) + 61 * ratio)
                raw_data.extend([r, g, b, 255])
                
    # PNG signature
    png = bytearray(b'\x89PNG\r\n\x1a\n')
    
    # IHDR
    ihdr_data = struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)
    ihdr_crc = zlib.crc32(b'IHDR' + ihdr_data)
    png.extend(struct.pack('>I', 13) + b'IHDR' + ihdr_data + struct.pack('>I', ihdr_crc))
    
    # IDAT
    compressed = zlib.compress(raw_data)
    idat_crc = zlib.crc32(b'IDAT' + compressed)
    png.extend(struct.pack('>I', len(compressed)) + b'IDAT' + compressed + struct.pack('>I', idat_crc))
    
    # IEND
    iend_crc = zlib.crc32(b'IEND')
    png.extend(struct.pack('>I', 0) + b'IEND' + struct.pack('>I', iend_crc))
    
    return bytes(png)

sizes = [16, 32, 48, 128]
base_dir = os.path.dirname(os.path.abspath(__file__))

for size in sizes:
    png_bytes = create_png(size, size, (255, 69, 0), (184, 255, 61, 255))
    out_path = os.path.join(base_dir, f'icon{size}.png')
    with open(out_path, 'wb') as f:
        f.write(png_bytes)
    print(f"Generated {out_path} ({size}x{size})")
