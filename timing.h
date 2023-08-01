#ifndef _TIMING_H_
#define _TIMING_H_

#include <string>
#include <time.h>
#include <math.h>


// ---------------------------------------------------------------------------
//
// TIMER
//
// High-precision timer class (nanosecond)
//
// ---------------------------------------------------------------------------
class Timer {

  private:

    struct timespec     m_tic;
    double              m_dInterval;  // seconds
 
  public:

    // Constructor and destructor
    Timer() { 
      reset();
    }

    ~Timer() {}


    // Rest to zero
    void reset() {
      m_tic.tv_sec = 0; 
      m_tic.tv_nsec = 0; 
      m_dInterval = 0; 
    }      
    
    // Return the interval
    double get() const { return m_dInterval; }


    // Start the timer
    void tic() { 
      clock_gettime(CLOCK_MONOTONIC, &m_tic);
      m_dInterval = 0; } 


    // Record the difference between now and tic().  Updates each time it is
    // called, always comparing to the original tic() time.
    double toc() { 

      struct timespec toc;
      clock_gettime(CLOCK_MONOTONIC, &toc); 
        
      // Calculate the difference between tic and toc in seconds
      m_dInterval = (toc.tv_sec - m_tic.tv_sec) + (toc.tv_nsec - m_tic.tv_nsec) / 1e9; 
      
      // If there was no tic before our toc, throw it out
      if (m_dInterval < 0) {
        m_dInterval = 0;
      }      

      return m_dInterval;
    }
      
      
    // Record the difference between now and tic.  Only updates if toc()
    // toc_if_first() hasn't already been called.  Subsequent calls to toc()
    // will overwrite this interval, but subsequent calls to toc_if_first()
    // will not.
    double toc_if_first() {
      if (m_dInterval == 0) {
        toc();
      }
      return m_dInterval;
    }

}; // Timer
  



// ---------------------------------------------------------------------------
//
// TIMEKEEPER
//
// Wrapper class for date-time storage and simple manipulation.  The principal
// storage unit is seconds since 00:00:00 January 1, 1970.
//
// ---------------------------------------------------------------------------
class TimeKeeper {
  
  private:

    // Member variables
    int     m_iYear;
    int     m_iDayOfYear;
    int     m_iHour;
    int     m_iMinutes;
    int     m_iSeconds;
    double  m_dSecondsSince1970;

  public:

    // Constructor and destructor
    TimeKeeper() : m_iYear(1970), m_iDayOfYear(1), 
                    m_iHour(0), m_iMinutes(0), m_iSeconds(0), 
                    m_dSecondsSince1970(0) { }
    
    ~TimeKeeper() { }

    // Operator functions
    TimeKeeper& operator=(const TimeKeeper& src) = default;
       
    friend bool operator==(const TimeKeeper& lhs, const TimeKeeper& rhs)
    {
       return (lhs.m_dSecondsSince1970 == rhs.m_dSecondsSince1970);
    }

    friend bool operator<(const TimeKeeper& lhs, const TimeKeeper& rhs)
    {
       return (lhs.m_dSecondsSince1970 < rhs.m_dSecondsSince1970);
    }
    
    friend bool operator> (const TimeKeeper& lhs, const TimeKeeper& rhs) { return (rhs < lhs); }
    friend bool operator<=(const TimeKeeper& lhs, const TimeKeeper& rhs) { return !(lhs > rhs); }
    friend bool operator>=(const TimeKeeper& lhs, const TimeKeeper& rhs) { return !(lhs < rhs); }
    friend bool operator!=(const TimeKeeper& lhs, const TimeKeeper& rhs) { return !(lhs == rhs); }


    // Public functions


    double set(double dSecondsSince1970) { 

      m_dSecondsSince1970 = dSecondsSince1970;
      getDateTimeFromSecondsSince1970(dSecondsSince1970, &m_iYear, &m_iDayOfYear, &m_iHour, &m_iMinutes, &m_iSeconds);

      return dSecondsSince1970;
    }


    // Set using a string with format:  YYYY/MM/DDTHH::MM::SS
    double set(const std::string& strDateTime) { 

      int month, day;

      sscanf(strDateTime.c_str(), "%4d/%2d/%2dT%2d:%2d:%2d", &m_iYear, &month, &day, &m_iHour, &m_iMinutes, &m_iSeconds);

      m_iDayOfYear = getDayOfYearFromDate(m_iYear, month, day);
      m_dSecondsSince1970 = getSecondsSince1970FromDateTime(m_iYear, m_iDayOfYear, m_iHour, m_iMinutes, m_iSeconds);

      return m_dSecondsSince1970;
    }


    // Set with the current system time
    double setNow() {

      struct tm *t;
      time_t tTime = time(NULL);
      t = gmtime (&tTime); // gmtime Jan 1 is day 0
      
      m_iYear = t->tm_year + 1900;
      m_iDayOfYear = t->tm_yday + 1;
      m_iHour = t->tm_hour;
      m_iMinutes = t->tm_min;
      m_iSeconds = t->tm_sec;

      m_dSecondsSince1970 = getSecondsSince1970FromDateTime(m_iYear, m_iDayOfYear, m_iHour, m_iMinutes, m_iSeconds);

      return m_dSecondsSince1970;
    }

    int year() const { return m_iYear;}
    
    int doy() const { return m_iDayOfYear;}
    
    int hh() const { return m_iHour;}

    int mm() const { return m_iMinutes;}

    int ss() const { return m_iSeconds;}

    double secondsSince1970() const { return m_dSecondsSince1970; }

    static void getDateTimeFromSecondsSince1970(double secs, 
                                                int *pyear, int *pday, 
                                                int *phr, int *pmin, 
                                                int *psec) {
      
      double days, day, sec;
      int i;

      day = floor (secs / 86400.0);
      sec = secs - day * 86400.0;
      for (i = 1970; day > 365; i++) {      
        days = ((i % 4 == 0 && i % 100 != 0) || i % 400 == 0) ? 366.0 : 365.0;
        day -= days;
      }

      *phr = (int) (sec / 3600.0);
      sec -= *phr * 3600.0;
      *pmin = (int) (sec / 60.0);
      *psec = (int) (sec - *pmin * 60);
      *pyear = i;
      day = day + 1;
      *pday = (int) day;

      if (day == 366) {        // fix for problem with day 366
        days = ((i % 4 == 0 && i % 100 != 0) || i % 400 == 0) ? 366 : 365;
        if (days == 365) {
          day -= 365;
          *pday = (int)day;
          *pyear = i + 1;
        }
      }
    }


    static double getSecondsSince1970FromDateTime(int year, int doy, int hour, int min, int sec) {

      double dSecondsSince1970 = (year - 1970) * 31536000.0 + (doy - 1) * 86400.0 + hour * 3600.0 + min * 60.0 + sec;

      for (int i = 1970; i < year; i++) {
        if ((i % 4 == 0 && i % 100 != 0) || i % 400 == 0) {
          dSecondsSince1970 += 86400.0;
        }
      }

      if (dSecondsSince1970 < 0.0) {
        dSecondsSince1970 = 0.0;
      }

      return dSecondsSince1970;
    }



    static int getDayOfYearFromDate(int year, int month, int day) {
      int i, leap;
      int daytab[2][13] = {
                {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
                {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31} };
      leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
      for (i = 1; i < month; i++) {
        day += daytab[leap][i];
      }

      return (day);
    }



    std::string getFileString(int uLevel) const
    {
      std::string sDateString;
      char txt[256];

      switch (uLevel) 
      {
      case 1:
        sprintf(txt, "%04d", m_iYear);
        break;
      case 2:
        sprintf(txt, "%04d_%03d", m_iYear, m_iDayOfYear);
        break;
      case 3:
        sprintf(txt, "%04d_%03d_%02d", m_iYear, m_iDayOfYear, m_iHour);
        break;
      case 4:
        sprintf(txt, "%04d_%03d_%02d_%02d", m_iYear, m_iDayOfYear, m_iHour, 
                m_iMinutes);
        break;
      default:
        sprintf(txt, "%04d_%03d_%02d_%02d_%02d", m_iYear, m_iDayOfYear, m_iHour, 
                m_iMinutes, m_iSeconds);
        break;
      }
      
      return std::string(txt);
    }



    std::string getDateTimeString(int uLevel) const
    {
      std::string sDateString;
      char txt[256];

      switch (uLevel) 
      {
      case 1:
        sprintf(txt, "%04d", m_iYear);
        break;
      case 2:
        sprintf(txt, "%04d-%03d", m_iYear, m_iDayOfYear);
        break;
      case 3:
        sprintf(txt, "%04d-%03d %02d", m_iYear, m_iDayOfYear, m_iHour);
        break;
      case 4:
        sprintf(txt, "%04d-%03d %02d:%02d", m_iYear, m_iDayOfYear, m_iHour, 
                m_iMinutes);
        break;
      default:
        sprintf(txt, "%04d-%03d %02d:%02d:%02d", m_iYear, m_iDayOfYear, m_iHour, 
                m_iMinutes, m_iSeconds);
      }
      
      return std::string(txt);
    }
};


#endif // _TIMING_H_

