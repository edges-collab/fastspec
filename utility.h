#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <string>
#include "timing.h"
#include "accumulator.h"


// ---------------------------------------------------------------------------
//
// Collection of helpful macros and functions
//
// ---------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Debug macros
// ----------------------------------------------------------------------------
#define EDGES_DEBUG

#ifdef EDGES_DEBUG
  #define debug(S) printf(S)
#else
  #define debug(S) 
#endif



// ----------------------------------------------------------------------------
// Disk IO functions
// ----------------------------------------------------------------------------
bool is_dir( const std::string& );

bool is_file( const std::string& );

bool make_path( const std::string&, mode_t);

std::string get_path( const std::string& );

std::string construct_filepath_name(const std::string&, 
                                    const std::string&, 
                                    const std::string&, 
                                    const std::string&);

bool write_switch_cycle( const std::string&, Accumulator&, Accumulator&, 
                         Accumulator& );

bool append_to_acq( const char*, const ACCUM_TYPE*, unsigned int, unsigned int, 
                    unsigned int, unsigned int, unsigned int, unsigned int,  
                    unsigned int, double, double, double, unsigned int, 
                    double, double, double, unsigned long);



// ----------------------------------------------------------------------------
// Math functions
// ----------------------------------------------------------------------------
template<typename T>
void get_blackman_harris(T* pOut, unsigned int uLength)
{
  const T a0 = 0.35875;
  const T a1 = 0.48829;
  const T a2 = 0.14128;
  const T a3 = 0.01168;

  for (unsigned int i = 0; i < uLength; i++) {
  
    pOut[i]     = a0 
                  - (a1 * cos( (2.0 * M_PI * i) / (uLength - 1) )) 
                  + (a2 * cos( (4.0 * M_PI * i) / (uLength - 1) )) 
                  - (a3 * cos( (6.0 * M_PI * i) / (uLength - 1) ));
  } 

} // get_blackman_harris



template<typename T>
void get_sinc(T* pOut, unsigned int uLength, unsigned int uWidth, bool bMultiplyInPlace)
{
  
  T halfLength = T(uLength)/2;
  T x;

  for (unsigned int i = 0; i < uLength; i++) {
    x = M_PI * (i - halfLength + 0.5) / uWidth;

    if (bMultiplyInPlace) {
      pOut[i] *= sin(x)/x;
    } else {
      pOut[i] = sin(x)/x;
    }
  } 

} // get_sinc






#endif // _UTILITY_H_

