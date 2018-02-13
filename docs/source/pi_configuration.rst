Raspberry Pi configuration
==========================

Power with USB to TTL serial cable connection
---------------------------------------------

* Pin 2: 5V(red)
* Pin 6: Gnd (Black)
* Pin 8 : Tx (White)
* Pin 10: Rx (Green)


Change keyboard layout
----------------------

Install required packages::

    sudo apt-get install console-data keyboard-configuration

Reconfigure and choose from the list::

    dpkg-reconfigure console-data
    dpkg-reconfigure keyboard-configuration
    service keyboard-setup restart


Create user
-----------
::

    useradd username -m -p password
    usermod -aG dialout,sudo username

Static ip
---------


Edit /etc/network/interfaces::

    auto lo
    iface lo inet loopback

    auto eth0
    iface eth0 inet static
       address X.X.X.X
       netmask 255.255.255.0
       gateway X.X.X.1


Change computer name
--------------------

Edit /etc/hosts::

    127.0.0.1     localhost
    127.0.1.1     COMPUTER_NAME


Edit /etc/hostname::

    COMPUTER_NAME


Add 1-Wire support
------------------

Start by adding the following line to **/boot/config.txt**

You can edit that file with nano by running sudo nano /boot/config.txt
and then scrolling to the bottom and typing it there

::

    dtoverlay=w1-gpio

Add the required modules at the bottom of **/etc/modules**

::

    w1-gpio
    w1-therm

reboot with **sudo reboot**

Test

::

    cd /sys/bus/w1/devices
    ls
    cd 28-xxxx (change this to match what serial number pops up)
    cat w1_slave


Script to add Arduino vendor info
---------------------------------

This is useful to upload Arduino code from the Pi to the Arduino board

Create a file get_arduino_id.sh, and copy this content::

    echo "Getting usb ids for Arduino…"
    # Search for the keyword Arduino and print the sixth column of that line
    vendorInfo=$(lsusb | awk '/Arduino/ {print $6}')
    idVendor=${vendorInfo:0:4}
    idProduct=${vendorInfo:5:4}
    symlink="USBT001"
    echo "Writting vendor info..."
    echo "SUBSYSTE  M=='tty', ATTRS{idVendor}==$idVendor, ATTRS{idProduct}==$idProduct, SYMLINK=$symlink" > /etc/udev/rules.d/99-usb_serial.rules

Make it executable::

    chmod +x get_arduino_id.sh

Connect your arduino board and launch the script::

    sh get_arduino_id.sh

