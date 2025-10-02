# ###########################
# EgoA DSP FX #
# Papa Fuzz Alpha v 0.7.1 (JUCE/CMake)
# Daniel Allen Rinker Oct 1 2025
# daniel.rinker@protonmail.ch
# https://:egoag.no
# ###########################

# check the license file on git
# you might have to install JUCE and/or xCode if it doesn't work

# - Defaults: Gain +6 dB, Bits = 6, Downsample = 4, +6 dB pre-drive into bitcrusher.

# To install as a VST or Logic/Garageband AU run the following in the terminal 
# Build:
```bash
cd PapaFuzz_v7_1
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
# you can stop here if you only need a VST
# you probably have to find where the file is and install it in your DAW

# To go further for Logic: Install AU (Logic):
```bash
sudo cp -rp "build/PapaFuzz_artefacts/Release/AU/Papa Fuzz.component" \
        /Library/Audio/Plug-Ins/Components/
auval -v aufx PFuz EgoA
```
