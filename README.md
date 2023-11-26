# sc4-city-date-sync

A DLL Plugin for SimCity 4 that synchronizes the city dates in a region.   

The plugin saves the current in-game date when exiting an established city, and then applying
it to the next established city if that city's in-game date is less then the stored value.
The city date is applied by automatically executing the SimDate cheat when loading the city.

## Download

The plugin can be downloaded from the Releases tab: https://github.com/0xC0000054/sc4-city-date-sync/releases

### Disclaimer:

I have not experienced any issues with using the SimDate command in this way, but it may have side effects that
I missed.
 

## System Requirements

* Windows 10 or later

## Installation

1. Close SimCity 4.
2. Copy `SC4CityDateSync.dll` and `SC4CityDateSync.ini` into the Plugins folder in the SimCity 4 installation directory.
3. Start SimCity 4.

## Troubleshooting

The plugin should write a `SC4CityDateSync.log` file in the same folder as the plugin.    
The log contains status information for the most recent run of the plugin.

# License

This project is licensed under the terms of the MIT License.    
See [LICENSE.txt](LICENSE.txt) for more information.

## 3rd party code

[gzcom-dll](https://github.com/nsgomez/gzcom-dll/tree/master) Located in the vendor folder, MIT License.    
[Windows Implementation Library](https://github.com/microsoft/wil) MIT License    

# Source Code

## Prerequisites

* Visual Studio 2022

## Building the plugin

* Open the solution in the `src` folder
* Update the post build events to copy the build output to you SimCity 4 application plugins folder.
* Build the solution

## Debugging the plugin

Visual Studio can be configured to launch SimCity 4 on the Debugging page of the project properties.
I configured the debugger to launch the game in a window with the following command line:    
`-intro:off -CPUcount:1 -w -CustomResolution:enabled -r1920x1080x32`

You may need to adjust the resolution for your screen.
