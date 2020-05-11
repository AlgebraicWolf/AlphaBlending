#include <iostream>
#include <fstream>
#include <memory>

using std::ifstream, std::unique_ptr;

class BitMapImage {
    struct CIEXYZ {
        unsigned int ciexyzX;
        unsigned int ciexyzY;
        unsigned int ciexyzZ;
    };

    struct CIEXYZTRIPLE {
        CIEXYZ ciexyzRed;
        CIEXYZ ciexyzGreen;
        CIEXYZ ciexyzBlue;
    };

private:
    unsigned int fileSize;
    unsigned int offBits;
    unsigned int structSize;
    int width;
    int height;
    unsigned short planes;
    unsigned short bitCount;
    unsigned int compression;
    unsigned int imageSize;
    int Xppm;
    int Yppm;
    unsigned int clrUsed;
    unsigned int clrImportant;
    unsigned int redMask;
    unsigned int greenMask;
    unsigned int blueMask;
    unsigned int alphaMask;
    unsigned int CSType;
    CIEXYZTRIPLE endpoints;
    unsigned int gammaRed;
    unsigned int gammaGreen;
    unsigned int gammaBlue;
    unique_ptr<unsigned char[]> image;

public:

    explicit BitMapImage(const char *filename);             // Default constructor loading image
    BitMapImage(const BitMapImage& other);                  // Copy constructor
    BitMapImage(BitMapImage&& other);                       // Move constructor
    BitMapImage& operator=(const BitMapImage& other);       // Copy assignment
    BitMapImage& operator=(BitMapImage&& other);            // Move assignment
    ~BitMapImage() noexcept ;                               // Destructor

    BitMapImage Blend(const BitMapImage& foreground);       // Use alpha-blending to add picture on top
    void Save(const char *filename);                        // Save BMP picture to file
};

BitMapImage::BitMapImage(const char *filename) {
    unique_ptr<ifstream> input = std::make_unique<ifstream>();
    input->open(filename, std::ios::binary | std::ios::in);

    unsigned short signature;
    *input >> signature;

    if(signature == 0x424d)
        throw std::runtime_error("Big-endian format is not yet supported");

    if(signature != 0x4d42)
        throw std::runtime_error("Invalid file signature");

    *input >> fileSize;

    input->ignore(4);        // Skip reserved zero fields

    *input >> offBits;          // Read offset to the beginning of the image
    *input >> structSize;       // Read structure size

    if(structSize != 108)
        throw std::runtime_error("Only BMP v4 is supported");

    *input >> width;            // Read image width
    *input >> height;           // Read image height
    *input >> planes;           // Read number of planes

    if(planes != 1)
        throw std::runtime_error("Invalid number of planes (Must be 1)");

    *input >> bitCount;         // Read depth of image

    if(bitCount != 32)
        throw std::runtime_error("Only 32-bit pixels are supported");

    *input >> compression;      // Read compression type

    if(compression != 6 && compression != 3)
        throw std::runtime_error("Only images with bitmask are supported");

    *input >> imageSize;        // Read image size
    *input >> Xppm;             // Read PPM for X axis
    *input >> Yppm;             // Read PPM for Y axis

    *input >> clrUsed;          // Read size of color table

    if(clrUsed != 0)
        throw std::runtime_error("Color table is not supported");

    *input >> clrImportant;     // Number of important colors in table

    *input >> redMask;          // Mask for red channel
    *input >> greenMask;        // Mask for green chanel
    *input >> blueMask;         // Mask for blue channel
    *input >> alphaMask;        // Mask for alpha channel

    *input >> CSType;           // Color space type

    if(!CSType)
        throw std::runtime_error("Custom color space is not supported");

    input->ignore(48);      // Skip color whatever

    size_t imagebytes = width * height * 4;

    image.reset((unsigned char *) aligned_alloc(32, imagebytes));
    input->read(reinterpret_cast<char *>(image.get()), imagebytes);
}

int main() {
    return 0;
}