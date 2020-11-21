# GVAR (EWS-G1, before known at GOES-13) Ingestor & Decoder

A program to receive and decode GVAR from older GOES satellites, more precisely EWS-G1 currently covering the indian ocean area, broadcasting imagery in L-Band on the GVAR downlink.

The downlink's frequency is 1685.7Mhz.   

***Huge thanks to @ZSztanga who made all of this possible by testing and providing all the data! Wouldn't have been possible without his immense help.***

**This software is still in beta, so bugs and potential instabilities are to be expected**

# Usage

The program can either run as a 24/7 service for automated station setups or as a standalone "one-time" decoder (processing a single .bin).

## Standalone

First of all, record a baseband at 3MSPS or more and demodulate with the included GVAR-Demodulator.grc (GNU Radio 3.8), obtaining a .bin.   
Then, run the .bin through the decoder as follow :
``` 
GVAR-Ingestor --dec gvar.bin
```

## Autonomous

In autonomous mode, the ingestor demodulates data from a SDR source, decodes it live and output images automatically. This most probably is the easiest way to go with even if you're not operating a 24/7 automated setup.   
While doing this on a Raspberry Pi 4 or similar should be doable, it has not been confirmed to work properly yet and may require some optimizations.

### Configuration

You will first need to create a config.yml in your work folder (where the binary is or will be executed), based on the template included in the repo.

```
# GVAR-Ingestor configuration

# SDR Settings
radio:
  device: "driver=airspy" # SoapySDR device string
  frequency: 1685700000 # EWS-G1 (GOES-13) Frequency
  sample_rate: 6000000 # Samplerate. Has to be >= 2.11MSPS
  gain: 40 # SDR Gain, an integer
  biastee: true

# Data director to save the pictures in
data_directory: GVAR_DATA

# Write demodulated data to pdr.bin (debugging only, ignore otherwise)
write_demod_bin: false
```

You will have to configure it accordingly for your setup / station. This default template is suitable for an Airspy-based setup, and should not require any modification for quick testing run apart from the gain.

Automated stations will likely want to set the data directory to be an absolute path.

### Running

Running the software is done with no argument, in which case it will default to demod + decode :
```
GVAR-Ingestor
```

If you're installing it on a fulltime operating station, you will want to start it inside a screen to keep it running when logging off, or create a systemd unit.   
Eg,
```
sudo apt install screen # Install the program on debian-based systems
screen -S gvar GVAR-Ingestor # Create a screen, press CTRL + A / D to detach
screen -x gvar # Attach to the screen
```

# Building

This will require [libpng](https://github.com/glennrp/libpng), [yamlcpp](https://github.com/jbeder/yaml-cpp), [SoapySDR](https://github.com/pothosware/SoapySDR) and [libdsp](https://github.com/altillimity/libdsp).   

If you are using a Debian-based system (Ubuntu, Raspberry PI, etc), you can just run those commands

```
sudo apt -y install libyamlcpp-dev libpng-dev libsoapysdr-dev cmake build-essential
git clone https://github.com/altillimity/libdsp.git
cd libdsp
mkdir build && cd build
cmake ..
make -j2
sudo make install
cd ../..
git clone https://github.com/altillimity/GVAR-Ingestor.git
cd GVAR-Ingestor
mkdir build && cd build
cmake ..
make -j2
sudo make install
```

Windows binaries are available in the repository with support for the following SDRs :
- Airspy
- RTL-SDR
   
If you need support for something else on Windows, feel free to open an issue and I will add it.