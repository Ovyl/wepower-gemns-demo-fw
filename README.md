# WePower BLE Demo Firmware
This project uses the BLE Beacon sample app as its base.  
We have made adjustments in the proj.conf file to optimize startup speed - mainly the RC oscillator and disabling the cryptocell.  

Requirements:  
- VS Code, NRF Connect Extension
- NCS Version 2.1.0  

To Build:
- Add Existing Application
- Navigate to this project  

App BLE data is configured to use Manufacturing Data, with a fully custom payload.   
Output power is +8dBm  
Advertising Interval is 20ms  

The app has 2 build configurations, one with logs enabled for debugging, and another that is optimized for low power. 