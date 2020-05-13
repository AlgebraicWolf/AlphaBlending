#include <cstdio>
#include <memory>
#include <cstring>
#include <immintrin.h>

const unsigned int BMP_FILE_HEADER_SIZE = 14;
const unsigned int BMP_V4_HEADER_SIZE = 108;
const unsigned int BMP_V5_HEADER_SIZE = 124;

using std::unique_ptr;

struct free_deleter {
    template <typename T>
    void operator()(T *p) const {
        std::free(const_cast<std::remove_const_t <T>*>(p));
    }
};

template<typename T>
void bufWrite(const unique_ptr<unsigned char []> &out, T value, size_t& offset) {
    memcpy(out.get() + offset, &value, sizeof(T));
    offset += sizeof(T);
}

class bufferWriter {
private:
    const unique_ptr<unsigned char []> &out;
    size_t offset;
public:
    bufferWriter(const unique_ptr<unsigned char []> &out) : out(out), offset(0) {}

    ~bufferWriter() = default;

    template<typename T>
    void operator()(T value) {
        bufWrite(out, value, offset);
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
    unique_ptr<unsigned char[], free_deleter> image;
public:

    explicit BitMapImage(const char *filename);                      // Default constructor loading image
    void deepCopy(const BitMapImage& other);                         // Actually copy assignment
    BitMapImage(BitMapImage &&other) noexcept;                       // Move constructor
    BitMapImage(const BitMapImage &other) = delete;                  // Implicit copying is prohibited
    BitMapImage &operator=(const BitMapImage &other) = delete;       // No implicit copying in order to avoid memory issues
    BitMapImage &operator=(BitMapImage &&other);                     // Move assignment
    ~BitMapImage() noexcept = default;                               // Destructor

    void Blend(const BitMapImage &foreground, unsigned int x, unsigned int y);       // Use alpha-blending to add picture on top
    void Save(const char *filename);                        // Save BMP picture to file
};

void BitMapImage::deepCopy(const BitMapImage &other) {
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

    image.reset(static_cast<unsigned char *>(aligned_alloc(32, width * height * 4)));
    memcpy(image.get(), other.image.get(), width * height * 4);
}



BitMapImage::BitMapImage(BitMapImage &&other) noexcept {
    std::swap(*this, other);
}

BitMapImage &BitMapImage::operator=(BitMapImage &&other) {
    std::swap(*this, other);
    return *this;
}

BitMapImage::BitMapImage(const char *filename) {
    unique_ptr<unsigned char[]> bitmapFileHeader = std::make_unique<unsigned char[]>(BMP_V4_HEADER_SIZE + BMP_FILE_HEADER_SIZE);

    unique_ptr<FILE, int(*)(FILE *)> input (fopen(filename, "rb"), &fclose);

    fread(bitmapFileHeader.get(), sizeof(unsigned char), BMP_V4_HEADER_SIZE + BMP_FILE_HEADER_SIZE, input.get());      // Read file header with BMP V4 Image header

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

    fileHeaderParser(width);                 // Read image width
    fileHeaderParser(height);           // Read image height
    fileHeaderParser(planes);           // Read number of planes

    if (planes != 1)
        throw std::runtime_error("Invalid number of planes (Must be 1)");

    fileHeaderParser(bitCount);         // Read depth of image

    if (bitCount != 32)
        throw std::runtime_error("Only 32-bit pixels are supported");

    fileHeaderParser(compression);      // Read compression type

    if (compression != 6 && compression != 3)
        throw std::runtime_error("Only images with bitmask are supported");

    fileHeaderParser(imageSize);        // Read image size
    fileHeaderParser(Xppm);             // Read PPM for X axis
    fileHeaderParser(Yppm);             // Read PPM for Y axis

    fileHeaderParser(clrUsed);          // Read size of color table

    if (clrUsed != 0)
        throw std::runtime_error("Color table is not supported");

    fileHeaderParser(clrImportant);     // Number of important colors in table

    fileHeaderParser(redMask);          // Mask for red channel
    fileHeaderParser(greenMask);        // Mask for green chanel
    fileHeaderParser(blueMask);         // Mask for blue channel
    fileHeaderParser(alphaMask);        // Mask for alpha channel

    fileHeaderParser(CSType);           // Color space type

    if (!CSType)
        throw std::runtime_error("Custom color space is not supported");

    offset += 48;      // Skip color whatever

    if (structSize == BMP_V5_HEADER_SIZE) {
        offBits -= 16;
        structSize = 108;
        fseek(input.get(), BMP_V5_HEADER_SIZE - BMP_V4_HEADER_SIZE, SEEK_CUR);    // Skip "redundant" bytes (F in chat)
    }


    image = unique_ptr<unsigned char[], free_deleter>(static_cast<unsigned char *>(aligned_alloc(32, width * height * 4)));

    fread(image.get(), sizeof(unsigned char), imageSize, input.get());
}

void BitMapImage::Save(const char *filename) {
    unique_ptr<unsigned char []> outBuffer (new unsigned char [BMP_FILE_HEADER_SIZE + BMP_V4_HEADER_SIZE]);

    auto writer = bufferWriter(outBuffer);

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

    unique_ptr<FILE, decltype(&fclose)> output(fopen(filename, "wb"), &fclose);
    fwrite(outBuffer.get(), sizeof(unsigned char), BMP_V4_HEADER_SIZE + BMP_FILE_HEADER_SIZE, output.get());
    fwrite(image.get(), sizeof(unsigned char), imageSize, output.get());
}

void BitMapImage::Blend(const BitMapImage &foreground, unsigned int x, unsigned int y) {
    unsigned char *bkg_ptr = image.get();
    unsigned char *frg_ptr = foreground.image.get();

    unsigned char bkg_line[32] = {};
    unsigned char bkg1_line[32] = {};
    unsigned char bkg2_line[32] = {};
    unsigned char frg_line[32] = {};
    unsigned char frg1_line[32] = {};
    unsigned char frg2_line[32] = {};
    unsigned char result[32] = {};

    for (unsigned int ycur = 0; ycur < foreground.height; ycur++) {
        for (unsigned int xcur = 0; xcur < foreground.width - 8; xcur+=8) {

            unsigned char diff1[32] = {};
            unsigned char diff2[32] = {};
            unsigned char alpha1[32] = {};
            unsigned char alpha2[32] = {};
//
            unsigned int bkg_pos = ((y + ycur) * width + x + xcur) << 2;
            unsigned int frg_pos = (ycur * foreground.width + xcur) << 2;

            /*
             *
             * Background: |A7|R7|G7|B7| |A6|R6|G6|B6| |A5|R5|G5|B5| |A4|R4|G4|B4| |A3|R3|G3|B3| |A2|R2|G2|B2| |A1|R1|G1|B1| |A0|R0|G0|B0|
             * Foreground: |A7|R7|G7|B7| |A6|R6|G6|B6| |A5|R5|G5|B5| |A4|R4|G4|B4| |A3|R3|G3|B3| |A2|R2|G2|B2| |A1|R1|G1|B1| |A0|R0|G0|B0|
             */
            for(unsigned int i = 0; i < 32; i++) bkg_line[i] = bkg_ptr[bkg_pos + i];
            for(unsigned int i = 0; i < 32; i++) frg_line[i] = frg_ptr[frg_pos + i];


            /*
             * Diff 1: |__A3|__R3| |__G3|__B3| |__A2|__R2| |__G2|__B2| |__A1|__R1| |__G1|__B1| |__A0|__R0| |__G0|__B0|
             * Diff 2: |__A7|__R7| |__G7|__B7| |__A6|__R6| |__G6|__B6| |__A5|__R5| |__G5|__B5| |__A4|__R4| |__G4|__B4|
             */

            for(unsigned int i = 0; i < 16; i++) frg1_line[2 * i] = frg_line[i];
            for(unsigned int i = 16; i < 32; i++) frg2_line[2 * i - 32] = frg_line[i];

            for(unsigned int i = 0; i < 16; i++) bkg1_line[2 * i] = bkg_line[i];
            for(unsigned int i = 16; i < 32; i++) bkg2_line[2 * i - 32] = bkg_line[i];

            for(unsigned int i = 0; i < 16; i++) reinterpret_cast<unsigned short *>(diff1)[i] = reinterpret_cast<unsigned short *>(frg1_line)[i] - reinterpret_cast<unsigned short *>(bkg1_line)[i];
            for(unsigned int i = 0; i < 16; i++) reinterpret_cast<unsigned short *>(diff2)[i] = reinterpret_cast<unsigned short *>(frg2_line)[i] - reinterpret_cast<unsigned short *>(bkg2_line)[i];

            /*
             * Prepare alphas
             * Alpha 1: |__A3|__A3| |__A3|__A3| |__A2|__A2| |__A2|__A2| |__A1|__A1| |__A1|__A1| |__A0|__A0| |__A0|__A0|
             * Alpha 2: |__A7|__A7| |__A7|__A7| |__A6|__A6| |__A6|__A6| |__A5|__A5| |__A5|__A5| |__A4|__A4| |__A4|__A4|
             */
            for(unsigned int i = 0; i < 16; i+=4) alpha1[2 * i] = frg_line[i + 3];
            for(unsigned int i = 0; i < 16; i+=4) alpha1[2 * i + 2] = frg_line[i + 3];
            for(unsigned int i = 0; i < 16; i+=4) alpha1[2 * i + 4] = frg_line[i + 3];
            for(unsigned int i = 0; i < 16; i+=4) alpha1[2 * i + 6] = frg_line[i + 3];

            for(unsigned int i = 16; i < 32; i+=4) alpha2[2 * i - 32] = frg_line[i + 3];
            for(unsigned int i = 16; i < 32; i+=4) alpha2[2 * i - 32 + 2] = frg_line[i + 3];
            for(unsigned int i = 16; i < 32; i+=4) alpha2[2 * i - 32 + 4] = frg_line[i + 3];
            for(unsigned int i = 16; i < 32; i+=4) alpha2[2 * i - 32 + 6] = frg_line[i + 3];

            /*
             * Multiply alphas
             */
            for(unsigned int i = 0; i < 16; i++)
                reinterpret_cast<unsigned short *>(diff1)[i] *= reinterpret_cast<unsigned short *>(alpha1)[i];

            for(unsigned int i = 0; i < 16; i++)
                reinterpret_cast<unsigned short *>(diff2)[i] *= reinterpret_cast<unsigned short *>(alpha2)[i];

            /*
             * Exctract result bytes from diffs
             */

            for(int i = 0; i < 16; i++) result[i] = diff1[2 * i + 1];
            for(int i = 16; i < 32; i++) result[i] = diff2[2 * i - 32 + 1];

            for(int i = 0; i < 32; i++) result[i] += bkg_line[i];

            /*
             * Insert proper alpha
             */
            for(int i = 0; i < 32; i+=4) result[i + 3] = bkg_line[i + 3];

            /*
             * Store
             */

            for(int i = 0; i < 32; i++) bkg_ptr[bkg_pos + i] = result[i];

//            unsigned char alpha = foreground.image[frg_pos + 3];
//            unsigned char dif2 = (frg_ptr[frg_pos + 2] - bkg_ptr[bkg_pos + 2]);
//            unsigned char dif1 = (frg_ptr[frg_pos + 1] - bkg_ptr[bkg_pos + 1]);
//            unsigned char dif = (frg_ptr[frg_pos + 0] - bkg_ptr[bkg_pos + 0]);
//            bkg_ptr[bkg_pos + 2] = (bkg_ptr[bkg_pos + 2]) + ((dif2 * alpha) >> 8);
//            bkg_ptr[bkg_pos + 1] = (bkg_ptr[bkg_pos + 1]) + ((dif1 * alpha) >> 8);
//            bkg_ptr[bkg_pos] = (bkg_ptr[bkg_pos]) + ((dif * alpha) >> 8);
        }
    }
}

int main() {
    BitMapImage bkg("Hood.bmp");
    BitMapImage frg("Cat.bmp");

    for(int i = 0; i < 50000; i++) {
        bkg.Blend(frg, 328, 245);
    }

    bkg.Save("blended.bmp");
}