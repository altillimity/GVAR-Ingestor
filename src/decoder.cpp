#include "decoder.h"
#include "processing/differentialencoding.h"
#include <filesystem>
#include <iostream>
#include <vector>

GVARDecoder::GVARDecoder(std::string out)
{
    // Counters and so
    writingImage = false;

    // Init thread pool
    imageSavingThreadPool = std::make_shared<ctpl::thread_pool>(1);

    // Init readers
    infraredImageReader1.startNewFullDisk();
    infraredImageReader2.startNewFullDisk();
    visibleImageReader.startNewFullDisk();

    // Processing buffer
    processing_buffer = new uint8_t[PROCESSING_BUFFER_SIZE + 1];

    // Init save folder
    output_folder = out;
    std::filesystem::create_directory(output_folder);
}

GVARDecoder::~GVARDecoder()
{
    delete[] processing_buffer;
}

std::string getGvarFilename(std::tm *timeReadable, int channel)
{
    std::string utc_filename = "EWS-G1_" + std::to_string(channel) + "_" +                                                                                  // Satellite name and channel
                               std::to_string(timeReadable->tm_year + 1900) +                                                                               // Year yyyy
                               (timeReadable->tm_mon + 1 > 9 ? std::to_string(timeReadable->tm_mon + 1) : "0" + std::to_string(timeReadable->tm_mon + 1)) + // Month MM
                               (timeReadable->tm_mday > 9 ? std::to_string(timeReadable->tm_mday) : "0" + std::to_string(timeReadable->tm_mday)) + "T" +    // Day dd
                               (timeReadable->tm_hour > 9 ? std::to_string(timeReadable->tm_hour) : "0" + std::to_string(timeReadable->tm_hour)) +          // Hour HH
                               (timeReadable->tm_min > 9 ? std::to_string(timeReadable->tm_min) : "0" + std::to_string(timeReadable->tm_min)) +             // Minutes mm
                               (timeReadable->tm_sec > 9 ? std::to_string(timeReadable->tm_sec) : "0" + std::to_string(timeReadable->tm_sec)) + "Z";        // Seconds ss
    return utc_filename;
}

void GVARDecoder::writeFullDisks()
{
    const time_t timevalue = time(0);
    std::tm *timeReadable = gmtime(&timevalue);
    std::string timestamp = std::to_string(timeReadable->tm_year + 1900) + "-" +
                            (timeReadable->tm_mon + 1 > 9 ? std::to_string(timeReadable->tm_mon + 1) : "0" + std::to_string(timeReadable->tm_mon + 1)) + "-" +
                            (timeReadable->tm_mday > 9 ? std::to_string(timeReadable->tm_mday) : "0" + std::to_string(timeReadable->tm_mday)) + "_" +
                            (timeReadable->tm_hour > 9 ? std::to_string(timeReadable->tm_hour) : "0" + std::to_string(timeReadable->tm_hour)) + "-" +
                            (timeReadable->tm_min > 9 ? std::to_string(timeReadable->tm_min) : "0" + std::to_string(timeReadable->tm_min));

    std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait a bit
    std::cout << "Full disk finished, saving at GVAR_" + timestamp + "..." << std::endl;

    std::filesystem::create_directory(output_folder + "/GVAR_" + timestamp);

    std::string disk_folder = output_folder + "/GVAR_" + timestamp;

    std::cout << "Resizing..." << std::endl;
    image1.resize(image1.width(), image1.height() * 1.75);
    image2.resize(image2.width(), image2.height() * 1.75);
    image3.resize(image3.width(), image3.height() * 1.75);
    image4.resize(image4.width(), image4.height() * 1.75);
    image5.resize(image5.width(), image5.height() * 1.75);

    std::cout << "Channel 1... " + getGvarFilename(timeReadable, 1) + ".png" << std::endl;
    image5.save_png(std::string(disk_folder + "/" + getGvarFilename(timeReadable, 1) + ".png").c_str());

    std::cout << "Channel 2... " + getGvarFilename(timeReadable, 2) + ".png" << std::endl;
    image1.save_png(std::string(disk_folder + "/" + getGvarFilename(timeReadable, 2) + ".png").c_str());

    std::cout << "Channel 3... " + getGvarFilename(timeReadable, 3) + ".png" << std::endl;
    image2.save_png(std::string(disk_folder + "/" + getGvarFilename(timeReadable, 3) + ".png").c_str());

    std::cout << "Channel 4... " + getGvarFilename(timeReadable, 4) + ".png" << std::endl;
    image3.save_png(std::string(disk_folder + "/" + getGvarFilename(timeReadable, 4) + ".png").c_str());

    std::cout << "Channel 5... " + getGvarFilename(timeReadable, 5) + ".png" << std::endl;
    image4.save_png(std::string(disk_folder + "/" + getGvarFilename(timeReadable, 5) + ".png").c_str());

    writingImage = false;
}

void GVARDecoder::forceWriteFullDisks()
{
    writingImage = true;

    // Backup images
    image1 = infraredImageReader1.getImage1();
    image2 = infraredImageReader1.getImage2();
    image3 = infraredImageReader2.getImage1();
    image4 = infraredImageReader2.getImage2();
    image5 = visibleImageReader.getImage();

    // Write those
    if (imageSavingFuture.valid())
        imageSavingFuture.get();
    imageSavingThreadPool->push([&](int) { writeFullDisks(); });

    // Reset readers
    infraredImageReader1.startNewFullDisk();
    infraredImageReader2.startNewFullDisk();
    visibleImageReader.startNewFullDisk();
}

#include <fstream>
std::ofstream testFile("test2.bin");

void GVARDecoder::processBuffer(uint8_t *buffer, int size)
{
    // Perform diff decoding (NRZ-S)
    nrzsDecode(buffer, size);

    // Run through the deframer
    gvarFrames = gvarDeframer.work(buffer, size);

    for (std::vector<uint8_t> &frame : gvarFrames)
    {
        // Derandomize this frame
        frameDerandomizer.derandData(frame.data(), 32786);

        // Get block number
        int block_number = frame[8];

        //if (block_number == 11)
        //{
        //   uint16_t product = frame[12] << 8 | frame[13];
        //   std::cout << product << std::endl;
        //   if (product == 15)
        //       testFile.write((char *)&frame.data()[8], 32786 - 8);
        // }

        // IR Channels 1-2 (Reader 1)
        if (block_number == 1)
        {
            // Read counter
            uint16_t counter = frame[105] << 6 | frame[106] >> 2;

            // Safeguard
            if (counter > 1353)
                continue;

            // Easy way of showing an approximate progress percentage
            if (!writingImage)
                std::cout << "\rApproximate full disk progress : " << round(((float)counter / 1353.0f) * 1000.0f) / 10.0f << "%     " << std::flush;

            // Process it!
            infraredImageReader1.pushFrame(frame.data(), block_number, counter);
        }
        // IR Channels 3-4 (Reader 2)
        else if (block_number == 2)
        {
            // Read counter
            uint16_t counter = frame[105] << 6 | frame[106] >> 2;

            // Safeguard
            if (counter > 1353)
                continue;

            // Process it!
            infraredImageReader2.pushFrame(frame.data(), block_number, counter);
        }
        // VIS 1
        else if (block_number >= 3 && block_number <= 10)
        {
            testFile.write((char *)&frame.data()[8], 32786 - 8);
            // Read counter
            uint16_t counter = frame[105] << 6 | frame[106] >> 2;

            // Safeguard
            if (counter > 1353)
                continue;

            visibleImageReader.pushFrame(frame.data(), block_number, counter);

            if (counter == 1353)
            {
                std::cout << "Full disk end detected!" << std::endl;

                if (!writingImage)
                {
                    writingImage = true;

                    // Backup images
                    image1 = infraredImageReader1.getImage1();
                    image2 = infraredImageReader1.getImage2();
                    image3 = infraredImageReader2.getImage1();
                    image4 = infraredImageReader2.getImage2();
                    image5 = visibleImageReader.getImage();

                    // Write those
                    if (imageSavingFuture.valid())
                        imageSavingFuture.get();
                    imageSavingThreadPool->push([&](int) { writeFullDisks(); });

                    // Reset readers
                    infraredImageReader1.startNewFullDisk();
                    infraredImageReader2.startNewFullDisk();
                    visibleImageReader.startNewFullDisk();
                }
            }
        }
    }
}