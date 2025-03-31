#ifndef _ACCUMULATOR_H_
#define _ACCUMULATOR_H_

#include <stdlib.h>
#include "timing.h"

// ---------------------------------------------------------------------------
//
// ACCUMULATOR
//
// Class that encapsulates a spectrum and some ancillary information.  It is
// intended to facilitate accumulating spectra in place through the "add" 
// member function. 
//
// ---------------------------------------------------------------------------

#define ACCUM_DATA_TYPE double

class Accumulator {

  private:

    // Member variables
    ACCUM_DATA_TYPE*     m_pSpectrum;
    unsigned int    m_uDataLength;
    unsigned int    m_uNumAccums;
    double          m_dADCmin;
    double          m_dADCmax;
    double          m_dStartFreq;
    double          m_dStopFreq;
    double          m_dChannelFactor;
    double          m_dTemperature;
    unsigned long   m_uDrops;
    unsigned int    m_uId;
    TimeKeeper      m_startTime;
    TimeKeeper      m_stopTime;

  public:

    // Constructor and destructor
    
    Accumulator() : m_pSpectrum(NULL), m_uDataLength(0), m_uNumAccums(0), 
                    m_dADCmin(0), m_dADCmax(0), m_dStartFreq(0), m_dStopFreq(0), 
                    m_dChannelFactor(0), m_dTemperature(0), m_uDrops(0),
                    m_uId(0) { }
    
    ~Accumulator()
    {
      if (m_pSpectrum) {
        free(m_pSpectrum);
        m_pSpectrum = NULL;
      }
    }


    // Public functions
    
    void add(double dValue) {
      for (unsigned int i=0; i<m_uDataLength; i++) {
        m_pSpectrum[i] += dValue;
      }
    }



    template<typename T>
    unsigned int add(const T* pSpectrum, unsigned int uLength, double dADCmin, double dADCmax) 
    {

      // Abort if there isn't valid data to include
      if ((pSpectrum == NULL) || (uLength != m_uDataLength)) {
        printf("Failed to add spectrum to accumulation.\n");
        return false;
      }

      // Update the ADC min/max record
      m_dADCmin = (dADCmin < m_dADCmin) ? dADCmin : m_dADCmin;
      m_dADCmax = (dADCmax > m_dADCmax) ? dADCmax : m_dADCmax;

      // Add the new spectrum to the accumulation 
      for (unsigned int n=0; n<uLength; n++) {
        m_pSpectrum[n] += pSpectrum[n];
      }

      return ++m_uNumAccums;
    }
    
    void addDrops(unsigned long uDrops) { m_uDrops += uDrops; }
    
    void clear() 
    {
      // Set the spectrum to zeros
      for (unsigned int i=0; i<m_uDataLength; i++) {
        m_pSpectrum[i] = 0;
      }

      // Set the supporting spectrum info parameters to zeros
      m_uNumAccums = 0;
      m_dADCmin = 0;
      m_dADCmax = 0;
      m_startTime.set(0);
      m_stopTime.set(0);
      m_dTemperature = 0;
      m_uDrops = 0;
    }

    bool combine(const Accumulator* pAccum) 
    {
      // Abort if there isn't valid data to include
      if ((pAccum->m_pSpectrum == NULL) || (pAccum->m_uDataLength != m_uDataLength)) {
        printf("Failed to combine accumulators.\n");
        return false;
      }

      // Take the outer bounds of the start and stop times
      if (pAccum->m_startTime < m_startTime) {
        m_startTime.set(pAccum->m_startTime.secondsSince1970());
      }

      if (pAccum->m_stopTime > m_stopTime) {
        m_stopTime.set(pAccum->m_stopTime.secondsSince1970());
      } 

      // Take the outer bounds of the ADC min/max records
      m_dADCmin = (pAccum->getADCmin() < m_dADCmin) ? pAccum->getADCmin() : m_dADCmin;
      m_dADCmax = (pAccum->getADCmax() > m_dADCmax) ? pAccum->getADCmax() : m_dADCmax;

      // Increment the block counter
      m_uNumAccums += pAccum->m_uNumAccums;

      // Add the new spectrum to the accumulation 
      for (unsigned int n=0; n<m_uDataLength; n++) {
        m_pSpectrum[n] += pAccum->m_pSpectrum[n];
      }

      // Add together the number of data drops
      m_uDrops += pAccum->m_uDrops;

      return true;
    }

    ACCUM_DATA_TYPE get(unsigned int iIndex) const
    { 
      if (m_pSpectrum) {
        return m_pSpectrum[iIndex]; 
      } else {
        return 0;
      }
    }

    ACCUM_DATA_TYPE getADCmin() const { return m_dADCmin; }
    
    ACCUM_DATA_TYPE getADCmax() const { return m_dADCmax; }
    
    ACCUM_DATA_TYPE getChannelFactor() const { return m_dChannelFactor; }

    bool getCopyOfSum(ACCUM_DATA_TYPE* pOut, unsigned int uLength) const
    { 
      if ((pOut == NULL) || (uLength != m_uDataLength)) {
        return false;
      }

      for (unsigned int i=0; i<m_uDataLength; i++) {
        pOut[i] = m_pSpectrum[i];
      }

      return true;
    }

    bool getCopyOfAverage(ACCUM_DATA_TYPE* pOut, unsigned int uLength) const
    { 
      double dNormalize = 0;

      if ((pOut == NULL) || (uLength != m_uDataLength)) {
        return false;
      }

      if (m_uNumAccums > 0) {
        dNormalize = 1.0 / (double) m_uNumAccums;
      }

      for (unsigned int i=0; i<m_uDataLength; i++) {
        pOut[i] = m_pSpectrum[i] * dNormalize;
      }

      return true;
    }
    
    unsigned int getDataLength() const { return m_uDataLength; }
    
    unsigned int getId() const { return m_uId; }

    unsigned int getNumAccums() const { return m_uNumAccums; }

    double getStartFreq() const { return m_dStartFreq; }
    
    double getStopFreq() const { return m_dStopFreq; }

    TimeKeeper getStartTime() const { return m_startTime; }
    
    TimeKeeper getStopTime() const { return m_stopTime; }
    
    ACCUM_DATA_TYPE* getSum() { return m_pSpectrum; }

    ACCUM_DATA_TYPE getTemperature() const { return m_dTemperature; }

    unsigned long getDrops() const {return m_uDrops; }

    void init(unsigned int uDataLength, double dStartFreq, double dStopFreq, 
              double dChannelFactor) 
    { 
      if (m_pSpectrum) {
        free(m_pSpectrum);
      }
      m_pSpectrum = (ACCUM_DATA_TYPE*) malloc(uDataLength * sizeof(ACCUM_DATA_TYPE));
      m_uDataLength = uDataLength;
      m_dStartFreq = dStartFreq;
      m_dStopFreq = dStopFreq;
      m_dChannelFactor = dChannelFactor;
      clear();
    }

    void multiply(double dValue) {
      for (unsigned int i=0; i<m_uDataLength; i++) {
        m_pSpectrum[i] *= dValue;
      }
    }

    void setADCmin(double dMin) {m_dADCmin = dMin;}

    void setADCmax(double dMax) {m_dADCmax = dMax;}

    void setStartTime() { m_startTime.setNow(); }
    
    void setStartTime(const TimeKeeper& tk) { m_startTime = tk; }
    
    void setStopTime() { m_stopTime.setNow(); }
    
    void setStopTime(const TimeKeeper& tk) { m_stopTime = tk; }

    void setTemperature(double dTemperature) { m_dTemperature = dTemperature; }

    void setDrops(unsigned long uDrops) { m_uDrops = uDrops; }
    
    void setId(unsigned int uId) { m_uId = uId; };


};


#endif // _ACCUMULATOR_H_

