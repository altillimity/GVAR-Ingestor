#pragma once

#include <string>
#include "image/infrared1_reader.h"
#include "image/infrared2_reader.h"
#include "image/visible_reader.h"
#include "processing/simpledeframer.h"
#include "processing/derand.h"
#include "ctpl/ctpl_stl.h"

class GVARDecoder
{
private:
    // Processing buffer
    const int PROCESSING_BUFFER_SIZE = 8192;
    uint8_t *processing_buffer;

    // Utils values
    bool writingImage;

    // Image readers
    InfraredReader1 infraredImageReader1;
    InfraredReader2 infraredImageReader2;
    VisibleReader visibleImageReader;

    // PseudoNoise (PN) derandomizer
    PNDerandomizer frameDerandomizer;

    // Deframer
    SimpleDeframer<uint64_t, 64, 262288, 0b0001101111100111110100000001111110111111100000001111111111111110> gvarDeframer; // Crude but works
    std::vector<std::vector<uint8_t>> gvarFrames;

    // Images used as a buffer when writing it out
    cimg_library::CImg<unsigned short> image1;
    cimg_library::CImg<unsigned short> image2;
    cimg_library::CImg<unsigned short> image3;
    cimg_library::CImg<unsigned short> image4;
    cimg_library::CImg<unsigned short> image5;

    // Saving is multithreaded
    std::shared_ptr<ctpl::thread_pool> imageSavingThreadPool;
    std::future<void> imageSavingFuture;

    // Output folder
    std::string output_folder;

private:
    void writeFullDisks();

public:
    GVARDecoder(std::string out);
    ~GVARDecoder();
    void processBuffer(uint8_t *buffer, int size);
    void forceWriteFullDisks();
};