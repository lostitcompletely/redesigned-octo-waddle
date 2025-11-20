OVERVIEW:

effects_separate directory separated has each effect that can be added individually to a signal.
effects directory stores all the .h files and the effects without the UI/menu and portaudio code.
controller.cpp includes some (not all) of the effects but others can be played using effects_separated,
but not simultaneously

EFFECTS:

Bitcrusher - reduces number of samples causing the signal to become distorted

Fuzz - uses a high pass filter to remove lower frequencies and clipping the waveform to add a lighter distortion

pingpong delay - uses 2 channels and adds delay to 1 channel which becomes part of the input of the other and vice versa

phaser - changes phase of some freqs using an all pass filter and adds it to original signal causing interference

spectral mirror - delays the signal and flips it to add on to the original signal

vibrato - oscillating frequency by using a delay buffer and a low frequency oscillator (LFO) to change the
          position of the delay.
