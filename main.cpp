#include <iostream>
#include <fstream>
#include <functional>
#include <memory>
#include <cstring>

using std::ifstream, std::ofstream, std::unique_ptr;

template<typename T>
void binWrite(const unique_ptr<ofstream> &out, T value) {
    out->write(reinterpret_cast<char *>(&value), sizeof(T));
}

class binaryWriter {
private:
    const unique_ptr<ofstream> &out;
public:
    binaryWriter(const unique_ptr<ofstream> &out) : out(out) {}

    ~binaryWriter() = default;

    template<typename T>
    void operator()(T value) {
        binWrite(out, value);
    }
};

template<typename T>
void parseValue(T &to, const unique_ptr<unsigned char[]> &arr, size_t &offset) {
    to = *reinterpret_cast<T *>(arr.get() + offset);
    offset += sizeof(T);
}

class parserWrapper {
private:
    size_t &offset;
    const unique_ptr<unsigned char[]> &arr;

public:
    template<typename T>
    void operator()(T &dst) {
        parseValue(dst, arr, offset);
    }

    parserWrapper(size_t &offset, const unique_ptr<unsigned char[]> &arr) : arr(arr), offset(offset) {}

    ~parserWrapper() = default;
};

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
    unique_ptr<unsigned char[]> image;

public:

    explicit BitMapImage(const char *filename);             // Default constructor loading image
    BitMapImage(const BitMapImage &other);                  // Copy constructor
    BitMapImage(BitMapImage &&other);                       // Move constructor
    BitMapImage &operator=(const BitMapImage &other);       // Copy assignment
    BitMapImage &operator=(BitMapImage &&other);            // Move assignment
    ~BitMapImage() noexcept;                               // Destructor

    void Blend(const BitMapImage &foreground, int x, int y);       // Use alpha-blending to add picture on top
    void Save(const char *filename);                        // Save BMP picture to file
};

BitMapImage::BitMapImage(const BitMapImage &other) : fileSize(other.fileSize), offBits(other.offBits),
                                                     structSize(other.structSize), width(other.width),
                                                     height(other.height), planes(other.planes),
                                                     bitCount(other.bitCount), compression(other.compression),
                                                     imageSize(other.imageSize), Xppm(other.Xppm),
                                                     Yppm(other.Yppm), clrUsed(other.clrUsed),
                                                     clrImportant(other.clrImportant), redMask(other.redMask),
                                                     greenMask(other.greenMask), blueMask(other.blueMask),
                                                     alphaMask(other.alphaMask), CSType(other.CSType),
                                                     image(reinterpret_cast<unsigned char *>(aligned_alloc(32, width *
                                                                                                               height *
                                                                                                               4))) {
    memcpy(image.get(), other.image.get(), width * height * 4);
}


BitMapImage::BitMapImage(BitMapImage &&other) : fileSize(other.fileSize), offBits(other.offBits),
                                                structSize(other.structSize), width(other.width),
                                                height(other.height), planes(other.planes),
                                                bitCount(other.bitCount), compression(other.compression),
                                                imageSize(other.imageSize), Xppm(other.Xppm),
                                                Yppm(other.Yppm), clrUsed(other.clrUsed),
                                                clrImportant(other.clrImportant), redMask(other.redMask),
                                                greenMask(other.greenMask), blueMask(other.blueMask),
                                                alphaMask(other.alphaMask), CSType(other.CSType) {
    image = std::move(other.image);

    other.fileSize = 0;
    other.offBits = 0;
    other.structSize = 0;
    other.width = 0;
    other.height = 0;
    other.planes = 0;
    other.bitCount = 0;
    other.compression = 0;
    other.imageSize = 0;
    other.Xppm = 0;
    other.Yppm = 0;
    other.clrUsed = 0;
    other.clrImportant = 0;
    other.redMask = 0;
    other.greenMask = 0;
    other.blueMask = 0;
    other.alphaMask = 0;
    other.CSType = 0;
}

BitMapImage& BitMapImage::operator=(const BitMapImage &other) {
    fileSize = other.fileSize;
    offBits = other.offBits;
    structSize = other.structSize;
    width = other.width;
    height = other.height;
    planes = other.planes;
    bitCount = other.bitCount;
    compression = other.compression;
    imageSize = other.imageSize;
    Xppm = other.Xppm;
    Yppm = other.Yppm;
    clrUsed = other.clrUsed;
    clrImportant = other.clrImportant;
    redMask = other.redMask;
    greenMask = other.greenMask;
    blueMask = other.blueMask;
    alphaMask = other.alphaMask;
    CSType = other.CSType;

    image.reset(reinterpret_cast<unsigned char *>(aligned_alloc(32, width * height * 4)));
    memcpy(image.get(), other.image.get(), width * height * 4);
}

BitMapImage::~BitMapImage() = default;

BitMapImage::BitMapImage(const char *filename) {
    unique_ptr<unsigned char[]> bitmapFileHeader = std::make_unique<unsigned char[]>(18);

    unique_ptr<ifstream> input = std::make_unique<ifstream>();
    input->open(filename, std::ios::binary | std::ios::in);

    input->read(reinterpret_cast<char *>(bitmapFileHeader.get()), 18);

    size_t offset = 0;

    unsigned short signature = 0;

    auto fileHeaderParser = parserWrapper(offset, bitmapFileHeader);

    fileHeaderParser(signature);

    if (signature == 0x424d)
        throw std::runtime_error("Big-endian format is not yet supported");

    if (signature != 0x4d42)
        throw std::runtime_error("Invalid file signature");

    fileHeaderParser(fileSize);

    offset += 4;

    fileHeaderParser(offBits);           // Read offset to the beginning of the image
    fileHeaderParser(structSize);       // Read structure size

    if (structSize < 108)
        throw std::runtime_error("Only BMP v4 and BMP v5 are supported");

    unique_ptr<unsigned char[]> bitmapHeader = std::make_unique<unsigned char[]>(structSize);

    input->read(reinterpret_cast<char *>(bitmapHeader.get()), structSize);

    offset = 0;
    auto imageHeaderParser = parserWrapper(offset, bitmapHeader);

    imageHeaderParser(width);                 // Read image width
    imageHeaderParser(height);           // Read image height
    imageHeaderParser(planes);           // Read number of planes

    if (planes != 1)
        throw std::runtime_error("Invalid number of planes (Must be 1)");

    imageHeaderParser(bitCount);         // Read depth of image

    if (bitCount != 32)
        throw std::runtime_error("Only 32-bit pixels are supported");

    imageHeaderParser(compression);      // Read compression type

    if (compression != 6 && compression != 3)
        throw std::runtime_error("Only images with bitmask are supported");

    imageHeaderParser(imageSize);        // Read image size
    imageHeaderParser(Xppm);             // Read PPM for X axis
    imageHeaderParser(Yppm);             // Read PPM for Y axis

    imageHeaderParser(clrUsed);          // Read size of color table

    if (clrUsed != 0)
        throw std::runtime_error("Color table is not supported");

    imageHeaderParser(clrImportant);     // Number of important colors in table

    imageHeaderParser(redMask);          // Mask for red channel
    imageHeaderParser(greenMask);        // Mask for green chanel
    imageHeaderParser(blueMask);         // Mask for blue channel
    imageHeaderParser(alphaMask);        // Mask for alpha channel

    imageHeaderParser(CSType);           // Color space type

    if (!CSType)
        throw std::runtime_error("Custom color space is not supported");

    offset += 48;      // Skip color whatever

    if (structSize == 124) {
        offBits -= 16;
        structSize = 108;
    }

    size_t imagebytes = width * height * 4;

    image.reset((unsigned char *) aligned_alloc(32, imagebytes));
    input->read(reinterpret_cast<char *>(image.get()), imagebytes);
}

void BitMapImage::Save(const char *filename) {
    unique_ptr<ofstream> output = std::make_unique<ofstream>();
    output->open(filename, std::ios::out | std::ios::binary);

    auto writer = binaryWriter(output);

    writer(static_cast<unsigned short>(0x4d42));      // Bitmap image signature
    writer(fileSize);                                    // Filesize
    writer(static_cast<unsigned int>(0));               // Reserved fields
    writer(offBits);                                     // Offset to the beginning of the image
    writer(structSize);                                  // Header structure size
    writer(width);
    writer(height);
    writer(planes);
    writer(bitCount);
    writer(compression);
    writer(imageSize);
    writer(Xppm);
    writer(Yppm);
    writer(clrUsed);
    writer(clrImportant);
    writer(redMask);
    writer(greenMask);
    writer(blueMask);
    writer(alphaMask);

    writer(CSType);

    writer(static_cast<unsigned long long> (0));
    writer(static_cast<unsigned long long> (0));
    writer(static_cast<unsigned long long> (0));
    writer(static_cast<unsigned long long> (0));
    writer(static_cast<unsigned long long> (0));
    writer(static_cast<unsigned long long> (0));

    output->write(reinterpret_cast<char *>(image.get()), width * height * 4);
}

void BitMapImage::Blend(const BitMapImage &foreground, int x, int y) {
    for (int ycur = 0; ycur < foreground.height; ycur++) {
        for (int xcur = 0; xcur < foreground.width; xcur++) {
            unsigned int red_bkg = image[((y + ycur) * width + x + xcur) * 4 + 2];
            unsigned int green_bkg = image[((y + ycur) * width + x + xcur) * 4 + 1];
            unsigned int blue_bkg = image[((y + ycur) * width + x + xcur) * 4];

            unsigned int red_frg = foreground.image[(ycur * foreground.width + xcur) * 4 + 2];
            unsigned int green_frg = foreground.image[(ycur * foreground.width + xcur) * 4 + 2];
            unsigned int blue_frg = foreground.image[(ycur * foreground.width + xcur) * 4 + 2];

            unsigned int alpha = foreground.image[(ycur * foreground.width + xcur) * 4 + 3];

            unsigned int red_res = (red_bkg * (255 - alpha) + red_frg * alpha) / 256;
            unsigned int green_res = (green_bkg * (255 - alpha) + green_frg * alpha) / 256;
            unsigned int blue_res = (blue_bkg * (255 - alpha) + blue_frg * alpha) / 256;

            image[((y + ycur) * width + x + xcur) * 4 + 2] = red_res;
            image[((y + ycur) * width + x + xcur) * 4 + 1] = green_res;
            image[((y + ycur) * width + x + xcur) * 4] = blue_res;
        }
    }
}

int main() {
    BitMapImage bkg("Table.BMP");
    BitMapImage frg("AskhatCat.BMP");

    bkg.Blend(frg, 300, 230);

    bkg.Save("blended.bmp");
}