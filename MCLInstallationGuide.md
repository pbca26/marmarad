![MarmaraCreditLoops Logo](https://raw.githubusercontent.com/marmarachain/marmara/master/MCL-Logo.png "Marmara Credit Loops Logo")

# Marmara v.1.0.1 Installation

There are two methods available to install Marmara Credit Loops smart chain.
## Installing Marmara Credit Loops Smart Chain Pre-compiled Binaries

One can download and unzip our pre-compiled binaries. This is the simplest method and hence requires no installation procedure.

For more information on this method, please see the link below.

**__TO-DO: Add a link to simple installations section for pre-compiled executables of MCL__**

## Building Marmara Credit Loops Smart Chain From Source

One may also build Marmara Credit Loops Smart Chain from source. This is not required, however building from source is considered as the best practice in a production environment, since this allows one to instantly update to the latest patches and upgrades.

### Linux

#### Requirements

    Linux (easiest with a Debian-based distribution, such as Ubuntu)
        For Ubuntu, we recommend using the 16.04 or 18.04 releases

    64-bit Processor

    Minimum 2 CPUs

    Minimum 4GB of free RAM

#### Get Started
Verify that your system is up to date.
```	
sudo apt-get update
sudo apt-get upgrade -y
```

#### Install the dependency packages

```	
sudo apt-get install build-essential pkg-config libc6-dev m4 g++-multilib autoconf libtool ncurses-dev unzip git python zlib1g-dev wget bsdmainutils automake libboost-all-dev libssl-dev libprotobuf-dev protobuf-compiler libgtest-dev libqt4-dev libqrencode-dev libsodium-dev libdb++-dev ntp ntpdate software-properties-common curl clang libcurl4-gnutls-dev cmake clang -y
```
This action takes some time, depending on your Internet connection. Let the process run in the background.

Once completed, follow the steps below to install Marmara Credit Loops smart chain.

#### Clone the Marmara Credit Loops Repository
```	
cd ~
git clone https://github.com/marmarachain/marmara komodo --branch master --single-branch
```
>Skip the following steps and jump to **__Fetch the Zcash Parameters__** step if the system you are using has 8 GB or more RAM.

#### Changing swap size to 4 GB
Swap is a space on a disk which is used when the amount of physical RAM memory is full. 
If the system has 4 GB of RAM, to avoid cases of running out of RAM, swap space needs to be configured.

>Check if OS already has swap enabled using ``` sudo swapon --show ```. 
If swapfile exists and is not configured as at least 4 GB then turn off the swap before configuring it by ```sudo swapoff /swapfile ```

Execute the following commands to allocate 4 GB of RAM for the swap space. 
```
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile 
sudo mkswap /swapfile 
sudo swapon /swapfile
```
#### Fetch the Zcash Parameters
```
cd komodo
./zcutil/fetch-params.sh
./zcutil/build.sh -j$(nproc)
```
**The following steps are optional to follow.**

##### Setting up Swappiness
Swappiness is the kernel parameter that defines how much (and how often) Linux kernel copies RAM contents to swap. 
The following sets up the parameter's value as “10”. Note that the higher the value of the swappiness parameter, the more aggressively Linux's kernel will swap.
```
sudo sysctl vm.swappiness=10 
```
>This setting will persist until the next reboot. We can set this value automatically at restart by adding the line to our /etc/sysctl.conf file:
>```sudo nano /etc/sysctl.conf``` vm.swappiness=10

#### Enabling UFW
This step is not required for installing MCL but can be used for server security purposes.

- Check the status of UFW by
```
sudo ufw status
```
- UFW package can be installed by executing the following command
```	
sudo apt install ufw
```
- Activate UFW and allow connections by executing the following commands
```
echo y | sudo ufw enable
sudo ufw allow ssh
sudo ufw allow "OpenSSH"
sudo ufw allow 33824
```
# Important Notice
This software is based on **Komodo** which is based on **zcash** and is considered experimental and is continously undergoing heavy development. 
**Marmara Credit Loops is experimental and a work-in-progress.** Use at your own risk.

At no point of time do the MCL and Komodo Platform developers take any responsibility for any damage out of the usage of this software.
	
# License
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
