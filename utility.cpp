#include <iostream>
#include <string>
#include <sys/stat.h> // stat
#include <errno.h>    // errno, ENOENT, EEXIST
#include <math.h>     // log10

#include "utility.h"

using namespace std;


// ----------------------------------------------------------------------------
// is_file -- Return true if file exists
// ----------------------------------------------------------------------------
bool is_file(const string& path)
{
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & S_IFREG) != 0;
}


// ----------------------------------------------------------------------------
// is_dir -- Return true if directory exists
//
// Adapted from code posted to stackoverflow.com
// /questions/675039/how-can-i-create-directory-tree-in-c-linux
// ----------------------------------------------------------------------------
bool is_dir(const string& path)
{
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
}


// ----------------------------------------------------------------------------
// get_path
// ----------------------------------------------------------------------------
string get_path(const string& sFilePath) 
{
  size_t iPos = sFilePath.find_last_of("/\\");
  if (iPos == string::npos) {
    return string();
  } 

  return sFilePath.substr(0, iPos);
}


// ----------------------------------------------------------------------------
// get_filepath_without_extension
// ----------------------------------------------------------------------------
string get_filepath_without_extension(const string& sFilePath)
{
  size_t iPos = sFilePath.find_last_of(".");
  if (iPos == string::npos) {
    return sFilePath;
  } 

  return sFilePath.substr(0, iPos);
}



// ----------------------------------------------------------------------------
// make_path -- Recursive function to create full path provided
//
// Adapted from code posted to stackoverflow.com
// /questions/675039/how-can-i-create-directory-tree-in-c-linux
// 
// mode_t mode is the file permission, e.g. 755
// ----------------------------------------------------------------------------
bool make_path(const string& path, mode_t mode)
{
  if (path.empty()) { 
    return true; 
  }

  int ret = mkdir(path.c_str(), mode);

  if (ret == 0)
      return true;

  switch (errno)
  {
  case ENOENT:
      // parent didn't exist, try to create it
      {
          size_t pos = path.find_last_of('/');
          if (pos == string::npos)
              return false;
          if (!make_path(path.substr(0, pos), mode))
              return false;
      }
      // now, try to create again

      return 0 == mkdir(path.c_str(), mode);

  case EEXIST:
      // done!
      return is_dir(path);

  default:
      return false;
  }
}


// ----------------------------------------------------------------------------
// construct_filepat_name
// ----------------------------------------------------------------------------
string construct_filepath_name(const string& sBase, 
                               const string& sDateString, 
                               const string& sLabel, 
                               const string& sSuffix)
{
  // NOTE: sDateString must be in a format given by a TimeKeeper object
  return sBase + "/" + sDateString.substr(0, 4) + "/" + sDateString + "_" + sLabel + sSuffix;
}




// ----------------------------------------------------------------------------
// write_for_plot() -- Writes all three spectra from full switch cycle to 
//                         a text file.  
// ----------------------------------------------------------------------------
bool write_plot_file( const string& sFilePath,
                      Accumulator& acc0, 
                      Accumulator& acc1, 
                      Accumulator& acc2, 
                      unsigned int uBin )
{

  FILE* file;
  ACCUM_DATA_TYPE* p0 = acc0.getSum();
  ACCUM_DATA_TYPE* p1 = acc1.getSum();
  ACCUM_DATA_TYPE* p2 = acc2.getSum();
  ACCUM_DATA_TYPE d0;
  ACCUM_DATA_TYPE d1;
  ACCUM_DATA_TYPE d2;
  double f;
  unsigned int count = 0;  

  double fstart = acc0.getStartFreq();
  double fstop = acc0.getStopFreq();
  double flength = (double) acc0.getDataLength();
  double df = (fstop-fstart) / flength;
  double accums = (double) acc0.getNumAccums();
  double ta = 0;

  if ((file = fopen (sFilePath.c_str(), "w")) == NULL) {
    printf ("Error writing to plot file.  Cannot write to: %s\n", sFilePath.c_str());
    return false;
  }

  // Loop over the spectrum and calculate and write the desired outputs
  // Col 1: frequency [MHz]
  // Col 2: 10*log10(p0)
  // Col 3: 10*log10(p1)
  // Col 4: 10*log10(p2)
  // Col 5: ta = 400*(p0-p1)/(p2-p1) + 300
  for (unsigned int i = 0; i<acc0.getDataLength(); i++) {

    // Bin together uBin values
    if (count == 0) {
      f = fstart+df*i;
      d0 = p0[i];
      d1 = p1[i];
      d2 = p2[i];
    } else if (count < uBin) {
      d0 += p0[i];
      d1 += p1[i];
      d2 += p2[i];
      f += fstart+df*i;
    }

    count++;

    // Store the binned values
    if (count == uBin) {

      count = 0;
      d0 = d0/uBin;
      d1 = d1/uBin;
      d2 = d2/uBin;
      f = f/uBin;
  
      ta = 400.0*(d0-d1)/(d2-d1)+300.0;

      fprintf(file, "%12g %12g %12g %12g %12g\n", 
        f, 10.0*log10(d0/accums), 10.0*log10(d1/accums), 10.0*log10(d2/accums), ta);
    }
  }

  fclose(file);
  return true;
}



// ----------------------------------------------------------------------------
// append_switch_cycle() -- Writes all three spectra from full switch cycle to 
//                         ACQ file.  
// ----------------------------------------------------------------------------
bool append_switch_cycle( const string& sFilePath,
                          Accumulator& acc0, 
                          Accumulator& acc1, 
                          Accumulator& acc2 )
{
  Accumulator* pAccum = NULL;
  TimeKeeper startTime = acc0.getStartTime();
  double dEffectiveChannelSize = 0;
  bool bResult = true;

  // Make sure path exists (create if not present)
  /*size_t iPos = sFilePath.find_last_of("/");
  if (iPos != string::npos) {
    if (!make_path(sFilePath.substr(0, iPos), 0775)) {
      return false;
    }
  }
  */
  if (!make_path(get_path(sFilePath), 0775)) {
    return false;
  }

  // Write each spectrum
  for (unsigned int i=0; i<3; i++) {

    switch (i)
    {
    case 0:
      pAccum = &acc0;
      break;
    case 1:
      pAccum = &acc1;
      break;
    case 2:
      pAccum = &acc2;
      break;
    }

    dEffectiveChannelSize = 1000.0
          * pAccum->getChannelFactor() 
          * (pAccum->getStopFreq() - pAccum->getStartFreq()) 
          / (double) pAccum->getDataLength();  // kHz

    bResult = bResult && append_switch_pos(sFilePath.c_str(), 
                pAccum->getSum(), pAccum->getDataLength(), i, 
                startTime.year(), startTime.doy(), 
                startTime.hh(), startTime.mm(), startTime.ss(),
                pAccum->getStartFreq(), pAccum->getStopFreq(), 
                dEffectiveChannelSize,
                pAccum->getNumAccums(), 
                pAccum->getADCmin(), pAccum->getADCmax(),
                pAccum->getTemperature(), 
                pAccum->getDrops() );

  }

  return bResult;

}



// ----------------------------------------------------------------------------
// append_switch_pos() -- Append spectrum to ACQ file
//
// We aim to write .acq files that are backward compatible with the old pxspec
// output.  That leads to a couple of non-intuitive outputs as noted below...
//
// Notes: 1) nblk = 2 * number_of_accumulations
//        2) The old pxspec code had an extra factor of 0.5 normalization in
//           the window function so adcmin and adcmax were half their actual
//           values.  Here, we record acdmin and adcmax as their true value
//           relative to the digitizer voltage range (assuming they are
//           reported correctly to the accumulator).  Also, adcmin and adcmax
//           were calculated after applying the window function in pxspec, so 
//           didn't report the min and max over all the entire samples without 
//           weighting, whereas now we calculate before applying the window.
//        3) The actual values written for the spectrum are normalized by a 
//           factor proportional to the number of samples per accumualtion, 
//           then converted to dB, then have an offset subtracted, then are
//           multiplied by 10^5 before being converted to integer for encoding. 
// ----------------------------------------------------------------------------
bool append_switch_pos( const char* pchFilename, 
                        const ACCUM_DATA_TYPE* pSpectrum, 
                        unsigned int uLength, 
                        unsigned int uSwitch, 
                        unsigned int year, unsigned int doy, 
                        unsigned int hh, unsigned int mm, 
                        unsigned int ss,
                        double dStartFreq, double dStopFreq, 
                        double dEffectiveChannelSize,
                        unsigned int uNumAccums, 
                        double dADCmin, double dADCmax, 
                        double dTemp, unsigned long uDrops ) 
{
  FILE *file = NULL;
  unsigned int i, j;
  int k;
  char txt[256];
  char b64[64];

  double dStepFreq = (dStopFreq - dStartFreq) / (double) uLength;
  //printf("ACQ: numAccums: %d, uLength: %d, val: %6.3f\n", uNumAccums, uLength, pSpectrum[12000]);

  // Define the encoding lookup table
  for (i = 0; i < 26; i++) {
    b64[i] = 'A' + i;
    b64[i + 26] = 'a' + i;
  }

  for (i = 0; i < 10; i++) {
    b64[i + 52] = '0' + i;
  }

  b64[62] = '+';
  b64[63] = '/';

  // Open the file for appending
  if (!is_file(pchFilename)) {
    printf ("Error appending to ACQ file.  File not found: %s\n", pchFilename);
    return false;
  }

  if ((file = fopen (pchFilename, "a")) == NULL) {
    printf ("Error appending to ACQ file.  Cannot write to: %s\n", pchFilename);
    return false;
  }

  // Write the ancillary data to the file
  // original acq file: fprintf (file, "# swpos %d resolution %8.3f adcmax %8.5f adcmin %8.5f temp %2.0f C nblk %d nspec %d\n",
  //          uSwitch, dEffectiveChannelSize, dADCmax, dADCmin, dTemp, uNumAccums, uLength);
  fprintf (file, "# swpos %d data_drops %8lu adcmax %8.5f adcmin %8.5f temp %2.0f C nblk %d nspec %d\n",
            uSwitch, uDrops / uLength / 2, dADCmax, dADCmin, dTemp, uNumAccums, uLength);

  // Write the spectrum preamble to the file
  fprintf (file, "%4d:%03d:%02d:%02d:%02d %1d %8.3f %8.6f %8.3f %4.1f spectrum ",
            year, doy, hh, mm, ss, uSwitch, dStartFreq, dStepFreq, dStopFreq, dADCmax);

  // nblk = [[2?]] * uNumAccums  (I'm not sure about if three is a factor of 2 difference or not)
  // From pxspec: output_spec = 10.0 * log10( data[i] / (nblk*nspec*2.0) ) - 38.3;
  double dOut = 0;

  // Encode and write the spectrum to the file
  for (i=0; i<uLength; i++) {

    // Convert to the values saved by the pxspec code to be backwards compatible
    if (i<10) {
      dOut = -199;
    } else {
      dOut = 10.0 * log10( pSpectrum[i] / ((double)uNumAccums*(double)uLength*(double)2.0) ) - 38.3;
    }

    //if (i==12000) { printf("%8.6f\n", dOut); }

    k = -(int)(dOut * 1e05);
    if (k > 16700000) {
      k = 16700000;
    }

    if (k < 0) {
      k = 0;
    }

    for (j=0; j<4; j++) {
      txt[j] = b64[k >> (18-j*6) & 0x3f];
    }
    txt[4] = 0;
    fprintf(file,"%s",txt);
  }
  fprintf (file,"\n");

  // Close the file
  fclose (file);

  return true;

} // write_to_acq

