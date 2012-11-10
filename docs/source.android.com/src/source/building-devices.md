<!--
   Copyright 2010 The Android Open Source Project

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
-->

# Building for devices #

This page complements the main page about [Building](building.html) with
information that is specific to individual devices.

The supported devices with the current release are the Galaxy Nexus, Motorola
Xoom, and Nexus S.

Galaxy Nexus is supported only in GSM/HSPA+ configuration "maguro" and only
if it was originally sold with a "yakju" or "takju" operating system.

The Motorola Xoom is supported in the Wi-fi configuration "wingray"
sold in the USA.

Nexus S is supported in the GSM configuration "crespo".

In addition, [PandaBoard](http://pandaboard.org) a.k.a. "panda" is supported
in the master branch only, but is currently considered experimental.
The specific details to use a PandaBoard with the Android Open-Source Project
are in the file `device/ti/panda/README` in the source tree.

Nexus One a.k.a. "passion" is obsolete, was experimental in gingerbread and
unsupported, and can't be used with newer versions of the Android Open-Source
Project.

Android Developer Phones (ADP1 and ADP2, a.k.a. "dream" and "sapphire") are
obsolete, were experimental and unsupported in froyo, and can't be used with
newer versions of the Android Open-Source Project.

No CDMA devices are supported in the Android Open-Source Project.

## Building fastboot and adb ##

If you don't already have those tools, fastboot and adb can be built with
the regular build system. Follow the instructions on the page about
[building](building.html), and replace the main `make` command with

    $ make fastboot adb

## Booting into fastboot mode ##

During a cold boot, the following key combinations can be used to boot into fastboot mode,
which is a mode in the bootloader that can be used to flash the devices:

Device   | Keys
---------|------
maguro   | Press and hold both *Volume Up* and *Volume Down*, then press and hold *Power*
panda    | Press and hold *Input*, then press *Power*
wingray  | Press and hold *Volume Down*, then press and hold *Power*
crespo   | Press and hold *Volume Up*, then press and hold *Power*
passion  | Press and hold the trackball, then press *Power*
sapphire | Press and hold *Back*, then press *Power*
dream    | Press and hold *Back*, then press *Power*

Also, on devices running froyo or later where adb is enabled,
the command `adb reboot bootloader` can be used to reboot from
Android directly into the bootloader with no key combinations.

## Unlocking the bootloader ##

It's only possible to flash a custom system if the bootloader allows it.

This is the default setup on ADP1 and ADP2.

On Nexus One, Nexus S, Xoom, and Galaxy Nexus,
the bootloader is locked by default. With the device in fastboot mode, the
bootloader is unlocked with

    $ fastboot oem unlock

The procedure must be confirmed on-screen, and deletes the user data for
privacy reasons. It only needs to be run once.

Note that on the Nexus S, Motorola Xoom and on Galaxy Nexus,
all data on the phone is erased, i.e. both the applications' private data
and the shared data that is accessible over USB, including photos and
movies. Be sure to make a backup of any precious files you have before
unlocking the bootloader.

On Nexus One, the operation voids the warranty and is irreversible.

On Nexus S, Xoom, and Galaxy Nexus,
the bootloader can be locked back with

    $ fastboot oem lock

Note that this erases user data on Xoom (including the shared USB data).

## Obtaining proprietary binaries ##

Starting with IceCreamSandwich, the Android Open-Source Project can't be used
from pure source code only, and requires additional hardware-related proprietary
libraries to run, specifically for hardware graphics acceleration.

Official binaries for Nexus S, Galaxy Nexus, and PandaBoard can be
downloaded from
[Google's Nexus driver page](https://developers.google.com/android/nexus/drivers),
which add access to additional hardware capabilities with non-Open-Source code.

When a device is suppoted in the master branch, the binaries for the most
recent numbered release are the ones that should be used in the master
branch.

There are no official binaries for Nexus One, ADP2 or ADP1.

### Extracting the proprietary binaries ###

Each set of binaries comes as a self-extracting script in a compressed archive.
After uncompressing each archive, run the included self-extracting script
from the root of the source tree, confirm that you agree to the terms of the
enclosed license agreement, and the binaries and their matching makefiles
will get installed in the `vendor/` hierarchy of the source tree.

### Cleaning up when adding proprietary binaries ###

In order to make sure that the newly installed binaries are properly
taken into account after being extracted, the existing output of any previous
build needs to be deleted with

    $ make clobber

## Picking and building the configuration that matches a device ##

The steps to configure and build the Android Open-Source Project
are described in the page about [Building](building.html).

The recommended builds for the various devices are available through
the lunch menu, accessed when running the `lunch` command with no arguments:

Device   | Branch                       | Build configuration
---------|------------------------------|------------------------
maguro   | android-4.0.4_r1.1 or master | full_maguro-userdebug
panda    | master                       | full_panda-userdebug
wingray  | android-4.0.4_r1.1 or master | full_wingray-userdebug
crespo   | android-4.0.4_r1.1 or master | full_crespo-userdebug
passion  | android-2.3.7_r1             | full_passion-userdebug
sapphire | android-2.2.3_r1             | full_sapphire-userdebug
dream    | android-2.2.3_r1             | full_dream-userdebug

## Flashing a device ##

Set the device in fastboot mode if necessary (see above).

Because user data is typically incompatible between builds of Android,
it's typically better to delete it when flashing a new system.

    $ fastboot erase cache
    $ fastboot erase userdata

An entire Android system can be flashed in a single command: this writes
the boot, recovery and system partitions together after verifying that the
system being flashed is compatible with the installed bootloader and radio,
and reboots the system.

    $ fastboot flashall

On all devices except passion,
the commands above can be replaced with a single command

    $ fastboot -w flashall

Note that filesystems created via fastboot on Motorola Xoom aren't working
optimally, and it is strongly recommended to re-create them through recovery

    $ adb reboot recovery

Once in recovery, open the menu (press Power + Volume Up), wipe the cache
partition, then wipe data.

### Nexus S and Galaxy Nexus Bootloader and Cell Radio compatibility ###

On Nexus S, and Galaxy Nexus, each version of Android has only
been thoroughly tested with on specific version of the underlying bootloader
and cell radio software.
However, no compatibility issues are expected when running newer systems
with older bootloaders and radio images according to the following tables.

Nexus S (worldwide version "XX", i9020t and i9023):

Android Version | Preferred Bootloader | Preferred Radio | Also possible
----------------|----------------------|-----------------|--------------
2.3 (GRH55)     | I9020XXJK1           | I9020XXJK8
2.3.1 (GRH78)   | I9020XXJK1           | I9020XXJK8
2.3.2 (GRH78C)  | I9020XXJK1           | I9020XXJK8
2.3.3 (GRI40)   | I9020XXKA3           | I9020XXKB1      | All previous versions
2.3.4 (GRJ22)   | I9020XXKA3           | I9020XXKD1      | All previous versions
2.3.5 (GRJ90)   | I9020XXKA3           | I9020XXKF1      | All previous versions
2.3.6 (GRK39F)  | I9020XXKA3           | I9020XXKF1      | All previous versions
4.0.3 (IML74K)  | I9020XXKL1           | I9020XXKI1      | All previous versions
4.0.4 (IMM76D)  | I9020XXKL1           | I9020XXKI1

Nexus S (850MHz version "UC", i9020a):

Android Version | Preferred Bootloader | Preferred Radio | Also possible
----------------|----------------------|-----------------|--------------
2.3.3 (GRI54)   | I9020XXKA3           | I9020UCKB2
2.3.4 (GRJ22)   | I9020XXKA3           | I9020UCKD1      | All previous versions
2.3.5 (GRJ90)   | I9020XXKA3           | I9020UCKF1      | All previous versions
2.3.6 (GRK39C)  | I9020XXKA3           | I9020UCKF1      | All previous versions
2.3.6 (GRK39F)  | I9020XXKA3           | I9020UCKF1      | All previous versions
4.0.3 (IML74K)  | I9020XXKL1           | I9020UCKF1      | All previous versions
4.0.4 (IMM76D)  | I9020XXKL1           | I9020UCKJ1

Nexus S (Korea version "KR", m200):

Android Version | Preferred Bootloader | Preferred Radio | Also possible
----------------|----------------------|-----------------|--------------
2.3.3 (GRI54)   | I9020XXKA3           | I9020KRKB3
2.3.4 (GRJ22)   | I9020XXKA3           | M200KRKC1       | All previous versions
2.3.5 (GRJ90)   | I9020XXKA3           | M200KRKC1       | All previous versions
2.3.6 (GRK39F)  | I9020XXKA3           | M200KRKC1       | All previous versions
4.0.3 (IML74K)  | I9020XXKL1           | M200KRKC1       | All previous versions
4.0.4 (IMM76D)  | I9020XXKL1           | M200KRKC1       | Versions from 2.3.6

Galaxy Nexus (GSM/HSPA+):

Android Version | Preferred Bootloader | Preferred Radio | Also possible
----------------|----------------------|-----------------|--------------
4.0.1 (ITL41D)  | PRIMEKJ10            | I9250XXKK1
4.0.2 (ICL53F)  | PRIMEKK15            | I9250XXKK6      | All previous versions
4.0.3 (IML74K)  | PRIMEKL01            | I9250XXKK6      | All previous versions
4.0.4 (IMM76D)  | PRIMEKL03            | I9250XXLA02

If you're building a new version of Android, if your Nexus S or
Galaxy Nexus has
an older bootloader and radio image that is marked as being also possible in
the table above but is not recognized by fastboot, you can locally
delete the `version-bootloader` and `version-baseband` lines in
`device/samsung/crespo/board-info.txt` or
`device/samsung/maguro/board-info.txt`

## Restoring a device to its original factory state ##

Factory images
for Galaxy Nexus (GSM/HSPA+ "yakju" and "takju", and CDMA/LTE "mysid")
and
for Nexus S (all variants)
are available from
[Google's factory image page](https://developers.google.com/android/nexus/images).

Factory images for the Motorola Xoom are distributed directly by Motorola.

No factory images are available for Nexus One.
