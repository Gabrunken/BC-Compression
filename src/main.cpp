#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_DXT_IMPLEMENTATION
#include <stb_dxt.h>
#include <iostream>
#include <string>
#include <vector>     // Necessario per il parsing dinamico
#include <stdint.h>
#include <cstring>

#define NO_FILE_PATH_DEFINED 1
#define WRONG_FILE_PATH 2
#define BAD_RESOLUTION 3
#define OUTPUT_FILE_FAIL 4
#define INVALID_TEXTURE_TYPE 5

struct TextureHeader
{
public:
    uint32_t magic;  // Un "numero magico" per riconoscere il formato (es. 'T', 'E', 'X', 'T')
    uint32_t width;  // Larghezza dell'immagine
    uint32_t height; // Altezza dell'immagine
    uint32_t format; // Identificativo formato: 1 per DXT1, 5 per DXT5
};

int main(int argc, char** argv)
{
    bool flipVertically = false;
    std::vector<std::string> args;

    // 1. Parsing dinamico di TUTTI gli argomenti
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-h") == 0)
        {
            printf("=== Asset Cooker - Texture Compression Tool ===\n");
            printf("Usage: %s [-f] <input.png> <output.bigcbc> [type]\n\n", argv[0]);
            printf("Options:\n");
            printf("  -f           : Flips the image vertically before compressing.\n");
            printf("                 (Required for OpenGL's bottom-left origin coordinate system)\n\n");
            printf("Available Texture Types:\n");
            printf("  albedo       : (Default) Base color without transparency. (BC1/DXT1)\n");
            printf("  albedo_alpha : Base color WITH transparency. (BC3/DXT5)\n");
            printf("  normal       : Normal maps. Applies DXT5nm optimization. (BC3/DXT5)\n");
            printf("  orm          : Packed masks (AO, Roughness, Metallic). (BC1/DXT1)\n");
            printf("=================================================\n");
            return 0;
        }
        else if (strcmp(argv[i], "-f") == 0)
        {
            flipVertically = true; // Flag intercettato!
        }
        else
        {
            // Se non è un flag, è un parametro di testo (file o tipo)
            args.push_back(argv[i]);
        }
    }

    // 2. Controllo dei file (ora usiamo args.size() invece di argc)
    if (args.size() < 2)
    {
        printf("Usage: %s [-f] <input.png> <output.bigcbc> [type: albedo|albedo_alpha|normal|orm]\n", argv[0]);
        printf("Type '%s -h' for more detailed information.\n", argv[0]);
        return NO_FILE_PATH_DEFINED;
    }

    // Estraiamo i parametri puliti dal nostro vector
    const char *inputFile = args[0].c_str();
    const char *outputFile = args[1].c_str();
    const char *texType = (args.size() > 2) ? args[2].c_str() : "albedo";

    // 3. Validazione rigorosa dell'input
    if (strcmp(texType, "albedo") != 0 &&
        strcmp(texType, "albedo_alpha") != 0 &&
        strcmp(texType, "normal") != 0 &&
        strcmp(texType, "orm") != 0)
    {
        printf("Error: Unknown texture type '%s'.\n", texType);
        printf("Allowed types: albedo, albedo_alpha, normal, orm\n");
        return INVALID_TEXTURE_TYPE;
    }

    bool isDXT5 = (strcmp(texType, "albedo_alpha") == 0 || strcmp(texType, "normal") == 0);
    bool isNormalMap = (strcmp(texType, "normal") == 0);

    // --- 4. MAGIA DEL FLIP VERTICALE ---
    // Diciamo a stb_image di capovolgere l'immagine in RAM durante il caricamento
    stbi_set_flip_vertically_on_load(flipVertically);
    // -----------------------------------

    int width, height, channels;
    unsigned char *img_data = stbi_load(inputFile, &width, &height, &channels, 4);

    if (!img_data)
    {
        printf("Error: failed to load %s\n", inputFile);
        return WRONG_FILE_PATH;
    }

    if (width % 4 != 0 || height % 4 != 0)
    {
        printf("Error: image resolution must be a power of 4 (%dx%d).\n", width, height);
        stbi_image_free(img_data);
        return BAD_RESOLUTION;
    }

    FILE *outFile = fopen(outputFile, "wb");
    if (!outFile)
    {
        printf("Error: couldn't create the output file.\n");
        stbi_image_free(img_data);
        return OUTPUT_FILE_FAIL;
    }

    TextureHeader header{};
    header.magic = 0x54584554;
    header.width = width;
    header.height = height;
    header.format = isDXT5 ? 5 : 1;

    fwrite(&header, sizeof(TextureHeader), 1, outFile);

    unsigned char block_rgba[64];
    unsigned char compressed_block[16];

    int blocks_x = width / 4;
    int blocks_y = height / 4;

    for (int by = 0; by < blocks_y; by++) {
        for (int bx = 0; bx < blocks_x; bx++) {

            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int px = (bx * 4) + x;
                    int py = (by * 4) + y;

                    int src_index = (py * width + px) * 4;
                    int dst_index = (y * 4 + x) * 4;

                    block_rgba[dst_index + 0] = img_data[src_index + 0];
                    block_rgba[dst_index + 1] = img_data[src_index + 1];
                    block_rgba[dst_index + 2] = img_data[src_index + 2];
                    block_rgba[dst_index + 3] = img_data[src_index + 3];
                }
            }

            if (isDXT5) {
                if (isNormalMap) {
                    for(int i = 0; i < 16; i++) {
                        block_rgba[i*4 + 3] = block_rgba[i*4 + 0];
                        block_rgba[i*4 + 0] = 0;
                        block_rgba[i*4 + 2] = 0;
                    }
                }

                stb_compress_dxt_block(compressed_block, block_rgba, 1, STB_DXT_NORMAL);
                fwrite(compressed_block, 1, 16, outFile);
            } else {
                stb_compress_dxt_block(compressed_block, block_rgba, 0, STB_DXT_NORMAL);
                fwrite(compressed_block, 1, 8, outFile);
            }
        }
    }

    fclose(outFile);
    stbi_image_free(img_data);

    std::printf("Asset cooked successfully: %s -> %s (Format: %s, Flipped: %s)\n",
                inputFile, outputFile, texType, flipVertically ? "Yes" : "No");
    return 0;
}