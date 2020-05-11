#include <iostream>
#include <fstream>
#include <functional>
#include <memory>

using std::ifstream, std::ofstream, std::unique_ptr;

template<typename T>
void binWrite(const unique_ptr<ofstream>& out, T value) {
    out->write(reinterpret_cast<char *>(&value), sizeof(T));
}

class binaryWriter {
private:
    const unique_ptr<ofstream>& out;
public:
    binaryWriter(const unique_ptr<ofstream>& out) : out(out) {}
    ~binaryWriter() = default;

    template<typename T>
    void operator() (T value) {
        binWrite(out, value);
    }
};

template <typename T>
void parseValue(T& to, const unique_ptr<unsigned char []>& arr, size_t& offset) {
    to = *reinterpret_cast<T *>(arr.get() + offset);
    offset += sizeof(T);
}

class parserWrapper {
private:
    size_t& offset;
    const unique_ptr<unsigned char []>& arr;

public:
    template<typename T>
    void operator()(T& dst) {
        parseValue(dst, arr, offset);
    }

    parserWrapper(size_t &offset, const unique_ptr<unsigned char []>& arr) : arr(arr), offset(offset) {}
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
    CIEXYZTRIPLE endpoints;
    unsigned int gammaRed;
    unsigned int gammaGreen;
    unsigned int gammaBlue;
    unique_ptr<unsigned char[]> image;

public:

    explicit BitMapImage(const char *filename);             // Default constructor loading image
    BitMapImage(const BitMapImage &other);                  // Copy constructor
    BitMapImage(BitMapImage &&other);                       // Move constructor
    BitMapImage &operator=(const BitMapImage &other);       // Copy assignment
    BitMapImage &operator=(BitMapImage &&other);            // Move assignment
    ~BitMapImage() noexcept;                               // Destructor

    BitMapImage Blend(const BitMapImage &foreground);       // Use alpha-blending to add picture on top
    void Save(const char *filename);                        // Save BMP picture to file
};

BitMapImage::~BitMapImage() = default;

BitMapImage::BitMapImage(const char *filename) {
    unique_ptr<unsigned char []> bitmapFileHeader = std::make_unique<unsigned char []>(14);

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

    unique_ptr<unsigned char []> bitmapHeader = std::make_unique<unsigned char []>(structSize);

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

    if(structSize == 124) {
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

int main() {
    BitMapImage img("in.bmp");
    img.Save("out.bmp");
}