#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_DXT_IMPLEMENTATION
#include <stb_dxt.h>
#include <iostream>
#include <string>
#include <stdint.h>
#include <cstring> // Necessario per strcmp()

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
    if (argc == 2 && strcmp(argv[1], "-h") == 0)
    {
        printf("=== Asset Cooker - Texture Compression Tool ===\n");
        printf("Usage: %s <input.png> <output.bin> [type]\n\n", argv[0]);
        printf("Available Texture Types:\n");
        printf("  albedo       : (Default) Base color without transparency.\n");
        printf("                 Compresses to BC1/DXT1 (8 bytes/block).\n\n");

        printf("  albedo_alpha : Base color WITH transparency (Alpha channel).\n");
        printf("                 Compresses to BC3/DXT5 (16 bytes/block).\n\n");

        printf("  normal       : Normal maps. Applies DXT5nm optimization (moves X to Alpha,\n");
        printf("                 clears R and B) to preserve maximum precision for lighting.\n");
        printf("                 Compresses to BC3/DXT5 (16 bytes/block). Calculate Z in shader.\n\n");

        printf("  orm          : Packed masks (R = Ambient Occlusion, G = Roughness, B = Metallic).\n");
        printf("                 Compresses to BC1/DXT1 (8 bytes/block). Load as Linear, NOT sRGB!\n");
        printf("=================================================\n");
        return 0; // Ritorniamo 0 perché è una richiesta di aiuto andata a buon fine
    }

    // 1. Lettura argomenti
    if (argc < 3)
    {
        printf("Usage: %s <input.png> <output.bin> [type: albedo|albedo_alpha|normal|orm]\n", argv[0]);
        return NO_FILE_PATH_DEFINED;
    }

    const char *inputFile = argv[1];
    const char *outputFile = argv[2];
    const char *texType = (argc > 3) ? argv[3] : "albedo";

    // --- NUOVO: Validazione rigorosa dell'input ---
    if (strcmp(texType, "albedo") != 0 &&
        strcmp(texType, "albedo_alpha") != 0 &&
        strcmp(texType, "normal") != 0 &&
        strcmp(texType, "orm") != 0)
    {
        printf("Error: Unknown texture type '%s'.\n", texType);
        printf("Allowed types: albedo, albedo_alpha, normal, orm\n");
        return INVALID_TEXTURE_TYPE;
    }
    // ----------------------------------------------

    // 2. Determiniamo le flag operative per il ciclo di compressione
    // Ora siamo sicuri al 100% che texType sia uno dei 4 valori validi
    bool isDXT5 = (strcmp(texType, "albedo_alpha") == 0 || strcmp(texType, "normal") == 0);
    bool isNormalMap = (strcmp(texType, "normal") == 0);

    int width, height, channels;
    // Forziamo 4 canali (RGBA) perché stb_dxt richiede blocchi esatti di 64 byte
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

    // 3. Compilazione dell'Header dinamico
    TextureHeader header{};
    header.magic = 0x54584554;
    header.width = width;
    header.height = height;
    header.format = isDXT5 ? 5 : 1; // 5 per DXT5, 1 per DXT1

    fwrite(&header, sizeof(TextureHeader), 1, outFile);

    unsigned char block_rgba[64];
    // IMPORTANTE: Il buffer ora è di 16 byte per accogliere il DXT5 se necessario
    unsigned char compressed_block[16];

    int blocks_x = width / 4;
    int blocks_y = height / 4;

    for (int by = 0; by < blocks_y; by++) {
        for (int bx = 0; bx < blocks_x; bx++) {

            // Estrazione della griglia 4x4
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

            // 4. Compressione e scrittura con branching logico
            if (isDXT5) {
                if (isNormalMap) {
                    // Magia DXT5nm: Ottimizzazione canali per la Normal Map
                    for(int i = 0; i < 16; i++) {
                        block_rgba[i*4 + 3] = block_rgba[i*4 + 0]; // Sposta la X (Rosso) nel canale Alpha
                        block_rgba[i*4 + 0] = 0;                   // Azzera il Rosso per risparmiare dati
                        block_rgba[i*4 + 2] = 0;                   // Azzera la Z (Blu), nello shader farai Z = sqrt(1 - X^2 - Y^2)
                    }
                }

                // Il parametro '1' dice a stb_dxt di processare in DXT5 (16 byte per blocco)
                stb_compress_dxt_block(compressed_block, block_rgba, 1, STB_DXT_NORMAL);
                fwrite(compressed_block, 1, 16, outFile);
            } else {
                // Il parametro '0' dice a stb_dxt di processare in DXT1 (8 byte per blocco)
                stb_compress_dxt_block(compressed_block, block_rgba, 0, STB_DXT_NORMAL);
                fwrite(compressed_block, 1, 8, outFile);
            }
        }
    }

    fclose(outFile);
    stbi_image_free(img_data);

    std::printf("Asset cooked successfully: %s -> %s (Format: %s)\n", inputFile, outputFile, texType);
    return 0;
}