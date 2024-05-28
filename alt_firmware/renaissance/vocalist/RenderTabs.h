#ifndef RENDERTABS_H
#define RENDERTABS_H

extern "C" {

extern const unsigned char tab48426[];
extern const unsigned char tab47492[];
extern const unsigned char amplitudeRescale[];

// Used to decide which phoneme's blend lengths. The candidate with the lower score is selected.
// tab45856
extern const unsigned char blendRank[];


// Number of frames at the end of a phoneme devoted to interpolating to next phoneme's final value
//tab45696
extern const unsigned char outBlendLength[];


// Number of frames at beginning of a phoneme devoted to interpolating to phoneme's final value
// tab45776
extern const unsigned char inBlendLength[];


// Looks like it's used as bit flags
// High bits masked by 248 (11111000)
//
// 32: S*    241         11110001
// 33: SH    226         11100010
// 34: F*    211         11010011
// 35: TH    187         10111011
// 36: /H    124         01111100
// 37: /X    149         10010101
// 38: Z*    1           00000001
// 39: ZH    2           00000010
// 40: V*    3           00000011
// 41: DH    3           00000011
// 43: **    114         01110010
// 45: **    2           00000010
// 67: **    27          00011011
// 70: **    25          00011001
// tab45936
extern const unsigned char sampledConsonantFlags[];

//tab42240
extern const unsigned char sinus[];

//tab42496
extern const unsigned char rectangle[];


//tab42752
extern const unsigned char multtable[];

//random data ?
extern const unsigned char sampleTable[];

}


#endif
