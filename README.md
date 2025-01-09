# Win16NabuAdapter
Win16 version of the NABU Adapter

# Deficiencies 
* General sizing/scaling of the app
* Add ability to save configuration

# Building
Use OpenWatcom 1.9 for best results

# Running
* Copy to your 16 bit (or 32 bit) Windows PC
* Copy NABU cycles that contain PAK files to C:\cycle
* Run the application!

# Notes
* The application will look for either .nab or .pak files in the directory specified in the settings dialog
  * If said .nab or .pak file cannot be found, it will attempt to download it from the internet based on the host specified in the settings dialog
    * NOTE: The host must support http, this application will NOT use https for download!!
* If any issues arise with loading files, suggest clearing out any files in the directory specified in the settings dialog.
