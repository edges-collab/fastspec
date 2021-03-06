#include <string>
#include <stdio.h>      // printf
#include <math.h>       // sin


// Fast random number generation
static unsigned long xr=123456789;
static unsigned long yr=362436069;
static unsigned long zr=521288629;

unsigned long xorshf96() { //period 2^96-1
  unsigned long tr;
  xr ^= xr << 16;
  xr ^= xr >> 5;
  xr ^= xr << 1;
  tr = xr;
  xr = yr;
  yr = zr;
  zr = tr ^ xr ^ yr;
  return zr;
}



// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------
int main(int argc, char* argv[])
{   

  // -----------------------------------------------------------------------
  // Parse the command line 
  // -----------------------------------------------------------------------
  for(int i=0; i<argc; i++)
  {
    std::string sArg = argv[i];

    if (sArg.compare("-h") == 0) { 
      printf("Usage:  gensamples [uNumSamples] [dSamplesPerSecond] [dFreq1] [dAmp1] [dFreq2] [dAmp2] [dAmpNoise] [strFilepath]\n");
    }
  }

  unsigned long long uNumSamples = std::stoull(argv[1]);
  double dSamplesPerSecond = std::stod(argv[2]);
  double dFreqCW1 = std::stod(argv[3]);
  double dAmpCW1 = std::stod(argv[4]);
  double dFreqCW2 = std::stod(argv[5]);
  double dAmpCW2 = std::stod(argv[6]); 
  double dAmpNoise = std::stod(argv[7]);
  std::string strFilename = std::string(argv[8]);

  printf("uNumSamples: %llu\n", uNumSamples);
  printf("dSamplesPerSecond: %g\n", dSamplesPerSecond);
  printf("dFreq1: %g\n", dFreqCW1);
  printf("dAmpl1: %g\n", dAmpCW1);
  printf("dFreq2: %g\n", dFreqCW2);
  printf("dAmp2: %g\n", dAmpCW2);
  printf("dAmpNoise: %g\n", dAmpNoise);
  printf("strFilepath (output): %s\n", strFilename.c_str());

  // -----------------------------------------------------------------------
  // Open the output file
  // -----------------------------------------------------------------------
  FILE* pFile = fopen(strFilename.c_str(), "wb");
  if (!pFile) {
    return -1;
  }
  
  // -----------------------------------------------------------------------
  // Generate and write the samples 
  // -----------------------------------------------------------------------
  unsigned long long i;
  unsigned long uRandom;
  unsigned short* pPointer;
  double cw1[4];
  double cw2[4];
  unsigned short noise[4];

  double dCW1 = 2.0*M_PI*dFreqCW1/dSamplesPerSecond;
  double dCW2 = 2.0*M_PI*dFreqCW2/dSamplesPerSecond;

  //noise[0]=0; noise[1]=0; noise[2]=0; noise[3]=0;

  // Loop in increments of four to use an efficient random number generator
  for (i=0; i<uNumSamples; i+=4) {
    
    // Print status periodically
    if ((i%1000000) == 0) {
      printf("Sample %llu of %llu (%5.1f%%)\n", i, uNumSamples, 100.0*i/uNumSamples);
    }

    // Calculate four samples of a continuous wave
    cw1[0] = dAmpCW1 * sin(dCW1*i);
    cw1[1] = dAmpCW1 * sin(dCW1*(i+1));
    cw1[2] = dAmpCW1 * sin(dCW1*(i+2));
    cw1[3] = dAmpCW1 * sin(dCW1*(i+3));

    cw2[0] = dAmpCW2 * sin(dCW2*i);
    cw2[1] = dAmpCW2 * sin(dCW2*(i+1));
    cw2[2] = dAmpCW2 * sin(dCW2*(i+2));
    cw2[3] = dAmpCW2 * sin(dCW2*(i+3));    

    // Calculate four samples of gaussian noise
    
    uRandom = xorshf96(); 
    pPointer = (unsigned short*) &uRandom;
    noise[0] = 2.0 * dAmpNoise * (pPointer[0] & 0x7FFF);
    noise[1] = 2.0 * dAmpNoise * (pPointer[1] & 0x7FFF);
    noise[2] = 2.0 * dAmpNoise * (pPointer[2] & 0x7FFF);
    noise[3] = 2.0 * dAmpNoise * (pPointer[3] & 0x7FFF);
    
    // Add the three contributions together
    noise[0] += (1.0 - dAmpNoise) * 32768.5 + 32768.0 * (cw2[0] + cw1[0]);
    noise[1] += (1.0 - dAmpNoise) * 32768.5 + 32768.0 * (cw2[1] + cw1[1]);
    noise[2] += (1.0 - dAmpNoise) * 32768.5 + 32768.0 * (cw2[2] + cw1[2]);
    noise[3] += (1.0 - dAmpNoise) * 32768.5 + 32768.0 * (cw2[3] + cw1[3]);

    // Write samples
    fwrite(noise, sizeof(unsigned short), 4, pFile);
  }

  // Close the output file
  fclose(pFile);

  return 0;
}
