#include <iostream>
#include <memory>

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
    std::unique_ptr<unsigned char> image;

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

int main() {
    return 0;
}