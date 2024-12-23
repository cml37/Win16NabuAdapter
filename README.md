# Win16NabuAdapter
Win16 version of the NABU Adapter

# Deficiencies 
* Only supports PAK files at present
* Doesn't implement Time segment
* General sizing/scaling of the app
* When using RS232, on slower systems, I believe we are suffering buffer underruns or something of the sort
  * So far, I have not been able to run this on a system that is slower than a Pentium II
* Add ability to save configuration

# Building
Use OpenWatcom 2.0 beta

# Running
* Copy to your 16 bit (or 32 bit) Windows PC
* Copy NABU cycles that contain PAK files to C:\cycle
* Run the application!
