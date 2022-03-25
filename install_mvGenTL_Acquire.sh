#!/bin/bash
DEF_DIRECTORY=/opt/mvIMPACT_Acquire
DEF_DATA_DIRECTORY=${MVIMPACT_ACQUIRE_DATA_DIR:-/opt/mvIMPACT_Acquire/data}
PRODUCT=mvGenTL-Acquire
API=mvIMPACT_acquire
TARNAME=mvGenTL_Acquire
GEV_SUPPORT=undefined
U3V_SUPPORT=undefined
PCIE_SUPPORT=undefined
USE_DEFAULTS=NO
UNATTENDED_INSTALLATION=NO
MINIMAL_INSTALLATION=NO
APT_GET_EXTRA_PARAMS=
OS_NAME="unknown"
OS_VERSION="unknown"
OS_CODENAME="unknown"
KERNEL_VERSION="unknown"

# Define a variable for the ErrorCount and WarningCount and an array for both to summarize the kind of issue
let ERROR_NUMBER=0
let WARNING_NUMBER=0

# Define variables for colorized bash output
# Foreground
red=`tput setaf 1`
yellow=`tput setaf 3`
green=`tput setaf 10`
blue=`tput setaf 12`
bold=`tput bold`
# Background
greyBG=`tput setaf 7`
reset=`tput sgr0`

# Define the users real name if possible, to prevent accidental mvIA root ownership if script is invoked with sudo
if [ "$(which logname)" == "" ] ; then
    USER=$(whoami)
else
    if [ "$(logname 2>&1 | grep -c logname:)" == "1" ] ; then
        USER=$(whoami)
    else
        USER=$(logname)
    fi
fi

# If user is root, then sudo shouldn't be used
if [ "$USER" == "root" ] ; then
        SUDO=
else
        SUDO=$(which sudo)
fi

# Check whether if systemd is supported or not
# Get the command of process id 1 (should be systemd on most systems)
INIT_SYSTEM=$(ps --no-headers -o comm 1)

# Determine OS Name, version and Kernel version
function check_distro_and_version()
{
  if [ -f /etc/fedora-release ] ; then
    OS_NAME='Fedora'
    OS_VERSION=`cat /etc/redhat-release | sed s/.*release\ // | sed s/\ .*//`
  elif [ -f /etc/redhat-release ] ; then
    OS_NAME='RedHat'
    OS_VERSION=`cat /etc/redhat-release | sed s/.*release\ // | sed s/\ .*//`
  elif [ -f /etc/SuSE-release ] ; then
    OS_NAME='SuSE'
    OS_VERSION=`cat /etc/SuSE-release | tr "\n" ' ' | sed s/.*=\ //`
  elif [ -f /etc/mandrake-release ] ; then
    OS_NAME='Mandrake'
    OS_VERSION=`cat /etc/mandrake-release | sed s/.*release\ // | sed s/\ .*//`
  elif [ -x /usr/bin/lsb_release ] ; then
    OS_NAME="$(lsb_release -is)" #Ubuntu
    OS_VERSION="$(lsb_release -rs)"
    OS_CODENAME="$(lsb_release -cs)"
  elif [ -f /etc/debian_version ] ; then
    OS_NAME="Debian"
    OS_VERSION="$(cat /etc/debian_version)"
  fi
  KERNEL_VERSION=$(uname -r)
}    

function createSoftlink {
    if [ ! -e "$1/$2" ]; then
        echo "Error: File "$1/$2" does not exist, softlink cannot be created! "
        exit 1
    fi
    if ! [ -L "$1/$3" ]; then
        ln -fs $2 "$1/$3" >/dev/null 2>&1
        if ! [ -L "$1/$3" ]; then
            $SUDO ln -fs $2 "$1/$3" >/dev/null 2>&1
            if ! [ -L "$1/$3" ]; then
                echo "Error: Could not create softlink $1/$3, even with sudo!"
                exit 1
            fi
        fi
    fi
}

# Print out ASCII-Art Logo.
clear;
echo ""
echo ""
echo ""
echo ""
echo "                              ===     ===    .MMMO                             "
echo "                               ==+    ==     M         ,MMM   ?M MM,           "
echo "                               .==   .=+     M  MMM   M    M   M   M           "
echo "                                ==+  ==.     M    M   M ^^^    M   M           "
echo "           ..                   .== ,==       MMMM    'MMMM    M   M           "
echo " MMMM   DMMMMMM      MMMMMM      =====                                         "
echo " MMMM MMMMMMMMMMM :MMMMMMMMMM     ====          MMMMMMMMMMMM   MMM             "
echo " MMMMMMMMMMMMMMMMMMMMMMMMMMMMM                 MMMMMMMMMMMM   MMM              "
echo " MMMMMMM   .MMMMMMMM    MMMMMM                     MMM       MMM               "
echo " MMMMM.      MMMMMM      MMMMM                    MM7       MMM                "
echo " MMMMM       MMMMM       MMMMM                   MMM       IMM                 "
echo " MMMMM       MMMMM       MMMMM                  MMM       MMMMMMMMMM           "
echo " MMMMM       MMMMM       MMMMM                                                 "
echo " MMMMM       MMMMM       MMMMM       M     MMM    MM    M   M  MMM  MMMM   MMMM"
echo " MMMMM       MMMMM       MMMMM      M M   M   M  M   M  M   M   M   M   M  M   "
echo " MMMMM       MMMMM       MMMMM     M   M  M      M   M  M   M   M   MMM,   MMM "
echo " MMMMM       MMMMM       MMMMM     MMMMM  M   M  M  ,M  M   M   M   M   M  M   "
echo "                                   M   M  'MMM'   MMMM, 'MMM'  MMM  M   M  MMMM"
echo "===============================================================================" 
sleep 1

# Analyze the command line arguments and react accordingly
PATH_EXPECTED=NO
SHOW_HELP=NO
while [[ $# -gt 0 ]] ; do
  if [ "$1" == "-h" ] || [ "$1" == "--help" ] ; then
    SHOW_HELP=YES
    break
  elif [[ ( "$1" == "-u" || "$1" == "--unattended" ) && "$PATH_EXPECTED" == "NO" ]] ; then
    if [ "$MINIMAL_INSTALLATION" == "YES" ] ; then
      echo
      echo "WARNING: Unattended installation and minimal installation are mutually exclusive!"
      echo
      SHOW_HELP=YES
      break
    else
      UNATTENDED_INSTALLATION=YES
    fi
  elif [[ ( "$1" == "-m" || "$1" == "--minimal" ) && "$PATH_EXPECTED" == "NO" ]] ; then
    if [ "$UNATTENDED_INSTALLATION" == "YES" ] ; then
      echo
      echo "WARNING: Minimal installation and unattended installation are mutually exclusive!"
      echo
      SHOW_HELP=YES
      break
    else
      MINIMAL_INSTALLATION=YES
    fi
  elif [[ ( "$1" == "-p" || "$1" == "--path" ) && "$PATH_EXPECTED" == "NO" ]] ; then
    if [ "$2" == "" ] ; then
      echo
      echo "WARNING: Path option used with no defined path, will use: $DEF_DIRECTORY directory"
      echo
      SHOW_HELP=YES
      break
    else
      PATH_EXPECTED=YES
    fi
  elif [ "$PATH_EXPECTED" == "YES" ] ; then
    DEF_DIRECTORY=$1
    PATH_EXPECTED=NO
  elif [[ ( "$1" == "-ogev" || "$1" == "--only_gev" ) && "$PATH_EXPECTED" == "NO" ]] ; then
    GEV_SUPPORT=TRUE
    U3V_SUPPORT=FALSE
    PCIE_SUPPORT=FALSE
  elif [[ ( "$1" == "-ou3v" || "$1" == "--only_u3v" ) && "$PATH_EXPECTED" == "NO" ]] ; then
    GEV_SUPPORT=FALSE
    U3V_SUPPORT=TRUE
    PCIE_SUPPORT=FALSE
  elif [[ ( "$1" == "-onaos" || "$1" == "--only_naos" ) && "$PATH_EXPECTED" == "NO" ]] ; then
    GEV_SUPPORT=FALSE
    U3V_SUPPORT=FALSE
    PCIE_SUPPORT=TRUE
  else
    echo 'Please check your syntax and try again!'
    SHOW_HELP=YES
  fi
  shift
done

if [ "$SHOW_HELP" == "YES" ] ; then
  echo
  echo 'Installation script for the '$PRODUCT' driver.'
  echo
  echo "Default installation path: "$DEF_DIRECTORY
  echo "Usage:                     ./install_mvGenTL_Acquire.sh [OPTION] ... "
  echo "Example:                   ./install_mvGenTL_Acquire.sh -p /myPath -u"
  echo
  echo "Arguments:"
  echo "-h --help                  Display this help."
  echo "-p --path                  Set the directory where the files shall be installed."
  echo "-ogev --only_gev           Install only the GigE Vision related features of the driver."
  echo "-ou3v --only_u3v           Install only the USB3 Vision related features of the driver."
  echo "-onaos --only_naos         Install only the mvBlueNAOS camera related features of the driver."
  echo "-u --unattended            Unattended installation with default settings. By using"
  echo "                           this parameter you explicitly accept the EULA."
  echo "-m --minimal               Minimal installation. No tools or samples will be built, and"
  echo "                           no automatic configuration and/or optimizations will be done."
  echo "                           By using this parameter you explicitly accept the EULA."
  echo
  exit 1
fi

if [ "$UNATTENDED_INSTALLATION" == "YES" ] ; then
  echo
  echo "Unattended installation requested, no user interaction will be required and the"
  echo "default settings will be used."
  echo
  USE_DEFAULTS=YES
fi

if [ "$MINIMAL_INSTALLATION" == "YES" ] ; then
  echo
  echo "Minimal installation requested, no user interaction will be required, no tools or samples"
  echo "will be built and no automatic configurations or optimizations will be done."
  echo
  USE_DEFAULTS=YES
fi

# Get some details about the system
check_distro_and_version

# Get the targets platform and if it is called "i686" we know it is a x86 system, else it s x86_64
TARGET=$(uname -m)
if [ "$TARGET" == "i686" ]; then
   TARGET="x86"
fi

# PCIe support is only available for 64 bit platforms
if [ "$TARGET" != "x86_64"  ]; then
    PCIE_SUPPORT=FALSE
fi

# Get the source directory (the directory where the files for the installation are) and cd to it
# (The script file must be in the same directory as the source TGZ) !!!
if which dirname >/dev/null; then
    SCRIPTSOURCEDIR="$(dirname $(realpath $0))"
fi
if [ "$SCRIPTSOURCEDIR" != "$PWD" ]; then
   if [ "$SCRIPTSOURCEDIR" == "" ] || [ "$SCRIPTSOURCEDIR" == "." ]; then
      SCRIPTSOURCEDIR="$PWD"
   fi
   cd "$SCRIPTSOURCEDIR"
fi

# Set variables for GenICam and mvIMPACT_acquire for later use
if grep -q '/etc/ld.so.conf.d/' /etc/ld.so.conf; then
   GENICAM_LDSOCONF_FILE=/etc/ld.so.conf.d/genicam.conf
   ACQUIRE_LDSOCONF_FILE=/etc/ld.so.conf.d/acquire.conf
else
   GENICAM_LDSOCONF_FILE=/etc/ld.so.conf
   ACQUIRE_LDSOCONF_FILE=/etc/ld.so.conf
fi

# Make sure the environment variables are set at the next boot as well
if grep -q '/etc/profile.d/' /etc/profile; then
   GENICAM_EXPORT_FILE=/etc/profile.d/genicam.sh
   ACQUIRE_EXPORT_FILE=/etc/profile.d/acquire.sh
else
   GENICAM_EXPORT_FILE=/etc/profile
   ACQUIRE_EXPORT_FILE=/etc/profile
fi

# Get driver name, version, file
if [ "$( ls | grep -c 'mvGenTL_Acquire.*\.tgz' )" != "0" ] ; then
  TARNAME=`ls mvGenTL_Acquire*.tgz | tail -n 1 | sed -e s/\\.tgz//`
  TARFILE=`ls mvGenTL_Acquire*.tgz | tail -n 1`
  VERSION=`ls mvGenTL_Acquire*.tgz | tail -n 1 | sed -e s/\\mvGenTL_Acquire// | sed -e s/\\-$TARGET// | sed -e s/\\_ABI2-// | sed -e s/\\.tgz//` 
  ACT2=$API-$VERSION
  ACT=$API-$TARGET-$VERSION
fi

# Check if tar-file is correct for the system architecture
if [ "$TARGET" == "x86_64"  ]; then
  if [ "`echo $TARNAME | grep -c x86_ABI2`" != "0" ]; then
    echo "-----------------------------------------------------------------------------------"
    echo "${red}  ABORTING: Attempt to install 32-bit drivers in a 64-bit machine!  ${reset}" 
    echo "-----------------------------------------------------------------------------------"
    exit
  fi
fi
if [ "$TARGET" == "x86" ]; then
  if [ "`echo $TARNAME | grep -c x86_64_ABI2`" != "0" ]; then
    echo "-----------------------------------------------------------------------------------"
    echo "${red}  ABORTING: Attempt to install 64-bit drivers in a 32-bit machine!  ${reset}" 
    echo "-----------------------------------------------------------------------------------"
    exit
  fi
fi

# A quick check whether the Version has a correct format (due to other files being in the same directory..?)
if [ "$(echo $VERSION | grep -c '^[0-9]\{1,2\}\.[0-9]\{1,2\}\.[0-9]\{1,2\}')" == "0" ]; then
  echo "-----------------------------------------------------------------------------------"
  echo "${red}  ABORTING: Script could not determine a valid mvIMPACT Acquire *.tgz file!  " 
  echo "${reset}-----------------------------------------------------------------------------------"
  echo "  This script could not extract a valid version number from the *.tgz file"
  echo "  This script determined $TARFILE as the file containing the installation data."
  echo "  It is recommended that only this script and the correct *.tgz file reside in this directory."
  echo "  Please remove all other files and try again."
  exit
fi

# A quick check whether the user has been determined
if [ "$USER" == "" ]; then
  echo "-----------------------------------------------------------------------------------"
  echo "${red}  ABORTING: Script could not determine a valid user!  ${reset}" 
  echo "-----------------------------------------------------------------------------------"
  echo "  This script could not determine the user of this shell"
  echo "  Please make sure this is a valid login shell!"
  exit
fi

YES_NO=
# Ask whether to use the defaults or proceed with an interactive installation
if [ "$UNATTENDED_INSTALLATION" == "NO" ] && [ "$MINIMAL_INSTALLATION" == "NO" ] ; then
  echo
  echo "Would you like this installation to run in unattended mode?"
  echo "Using this mode you explicitly agree to the EULA(End User License Agreement)!"
  echo "No user interaction will be required, and the default settings will be used!"
  echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
  read YES_NO
else
  YES_NO=""
fi
if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
  USE_DEFAULTS=NO
else
  USE_DEFAULTS=YES
fi

YES_NO=
# Here we will ask the user if we shall start the installation process
echo
echo "-----------------------------------------------------------------------------------"
echo "${bold}Host System:${reset}"
echo "-----------------------------------------------------------------------------------"
echo
echo "${bold}OS:                             ${reset}"$OS_NAME
echo "${bold}OS Version:                     ${reset}"$OS_VERSION
echo "${bold}OS Codename:                    ${reset}"$OS_CODENAME
echo "${bold}Kernel:                         ${reset}"$KERNEL_VERSION
echo "${bold}Platform:                       ${reset}"$TARGET
echo
echo "-----------------------------------------------------------------------------------"
echo "${bold}Configuration:${reset}"
echo "-----------------------------------------------------------------------------------"
echo
echo "${bold}Installation for user:            ${reset}"$USER
echo "${bold}Installation directory:           ${reset}"$DEF_DIRECTORY
echo "${bold}Data directory:                   ${reset}"$DEF_DATA_DIRECTORY
echo "${bold}Source directory:                 ${reset}"$(echo $SCRIPTSOURCEDIR | sed -e 's/\/\.//')
echo "${bold}Version:                          ${reset}"$VERSION
echo "${bold}TAR-File:                         ${reset}"$TARFILE
echo
echo "${bold}ldconfig:"
echo "${bold}GenICam:                        ${reset}"$GENICAM_LDSOCONF_FILE
echo "${bold}mvIMPACT_acquire:               ${reset}"$ACQUIRE_LDSOCONF_FILE
echo
echo "${bold}Exports:"
echo "${bold}GenICam:                        ${reset}"$GENICAM_EXPORT_FILE
echo "${bold}mvIMPACT_acquire:               ${reset}"$ACQUIRE_EXPORT_FILE
echo 
echo "-----------------------------------------------------------------------------------"
echo
echo "Do you want to continue (default is 'yes')?"
echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
if [ "$USE_DEFAULTS" == "NO" ] ; then
  read YES_NO
else
  YES_NO=""
fi
echo

# If the user is choosing no, we will abort the installation, else we will start the process.
if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
  echo "Quit!"
  exit
fi

# End User License Agreement
YES_NO="r"
while [ "$YES_NO" == "r" ] || [ "$YES_NO" == "R" ]
do
  echo
  echo "Do you accept the End User License Agreement (default is 'yes')?"
  echo "Hit 'n' + <Enter> for 'no', 'r' + <Enter> to read the EULA or "
  echo "just <Enter> for 'yes'."
  if [ "$USE_DEFAULTS" == "NO" ] ; then
    read YES_NO
    if [ "$YES_NO" == "r" ] || [ "$YES_NO" == "R" ] ; then
    if [ "x$(which more)" != "x" ] ; then
      EULA_SHOW_COMMAND="more -d"
    else
      EULA_SHOW_COMMAND="cat"
    fi
    tar -xzf $TARFILE -C /tmp mvIMPACT_acquire-$VERSION.tar && tar -xf /tmp/mvIMPACT_acquire-$VERSION.tar -C /tmp mvIMPACT_acquire-$VERSION/doc/EULA.txt --strip-components=2 && rm /tmp/mvIMPACT_acquire-$VERSION.tar && $EULA_SHOW_COMMAND /tmp/EULA.txt && rm /tmp/EULA.txt && sleep 1
    # clear the stdin buffer in case user spammed the Enter key
    while read -r -t 0; do read -r; done
    fi
  else
    YES_NO=""
  fi
done

# If the user is choosing no, we will abort the installation, else we continue.
if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
  echo "Quit!"
  exit
fi

echo
echo   "-----------------------------------------------------------------------------------"
echo   "${bold}BY INSTALLING THIS SOFTWARE YOU HAVE AGREED TO THE EULA(END USER LICENSE AGREEMENT)${reset}"
echo   "-----------------------------------------------------------------------------------"
echo
 
# First of all ask whether to dispose of the old mvIMPACT Acquire installation
if [ "$MVIMPACT_ACQUIRE_DIR" != "" ]; then
  echo "Existing installation detected at: $MVIMPACT_ACQUIRE_DIR"
  echo "Do you want to keep this installation (default is 'yes')?"
  echo "If you select no, mvIMPACT Acquire will be removed for ALL installed products!"
  echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
  if [ "$USE_DEFAULTS" == "NO" ] ; then
    read YES_NO
  else
    YES_NO=""
  fi
  echo
  if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
    $SUDO rm -f /usr/bin/mvDeviceConfigure >/dev/null 2>&1
    $SUDO rm -f /usr/bin/mvIPConfigure >/dev/null 2>&1
    $SUDO rm -f /usr/bin/wxPropView >/dev/null 2>&1
    $SUDO rm -f /etc/ld.so.conf.d/acquire.conf >/dev/null 2>&1
    $SUDO rm -f /etc/ld.so.conf.d/genicam.conf >/dev/null 2>&1
    $SUDO rm -f /etc/profile.d/acquire.sh >/dev/null 2>&1
    $SUDO rm -f /etc/profile.d/genicam.sh >/dev/null 2>&1
    $SUDO rm -f /etc/udev/rules.d/51-mvbf.rules >/dev/null 2>&1
    $SUDO rm -f /etc/udev/rules.d/52-U3V.rules >/dev/null 2>&1
    $SUDO rm -f /etc/udev/rules.d/52-mvbf3.rules >/dev/null 2>&1
    $SUDO rm -f /etc/udev/rules.d/51-udev-pcie.rules >/dev/null 2>&1
    $SUDO rm -f /usr/local/bin/make_device.sh 2>&1
    $SUDO rm -f /usr/local/bin/device_namer.sh 2>&1
    $SUDO rm -f /etc/sysctl.d/62-buffers-performance.conf >/dev/null 2>&1
    $SUDO rm -f /etc/security/limits.d/acquire.conf >/dev/null 2>&1
    $SUDO rm -rf /etc/matrix-vision >/dev/null >/dev/null 2>&1
    $SUDO rm -rf $MVIMPACT_ACQUIRE_DIR >/dev/null 2>&1
    if [ $? == 0 ]; then
      echo "Previous mvIMPACT Acquire Installation ($MVIMPACT_ACQUIRE_DIR) removed successfully!"
    else
      echo "Error removing previous mvIMPACT Acquire Installation ($MVIMPACT_ACQUIRE_DIR)!"
      echo "$?"
    fi
    if [ "$INIT_SYSTEM" == "systemd" ] && [ -f "/etc/systemd/system/resize_usbfs_buffersize.service" ]; then
      $SUDO systemctl stop resize_usbfs_buffersize.service
      $SUDO systemctl disable resize_usbfs_buffersize.service
      $SUDO rm -f /etc/systemd/system/resize_usbfs_buffersize.service
      if [ $? == 0 ]; then
        echo "usbcore.usbfs_memory_mb systemd service (/etc/systemd/system/resize_usbfs_buffersize.service) disabled and removed successfully!"
      else
        echo "Error removing usbcore.usbfs_memory_mb systemd service (/etc/systemd/system/resize_usbfs_buffersize.service)!"
        echo "$?"
      fi
    fi
  else
    echo "Previous mvIMPACT Acquire Installation ($MVIMPACT_ACQUIRE_DIR) NOT removed!"
  fi
fi
 
# Determine whether mvGenTL_Acquire should support GEV, U3V and/or mvBlueNAOS device types on this system
echo ""
echo "Should mvGenTL_Acquire support GEV devices, such as mvBlueCOUGAR (default is 'yes')?"
echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."

if [ "$GEV_SUPPORT" == "undefined" ]; then
  if [ "$USE_DEFAULTS" == "NO" ] ; then
    read YES_NO
  else
    YES_NO=""
  fi
  if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
      GEV_SUPPORT=FALSE
  else
      GEV_SUPPORT=TRUE
  fi
fi
echo ""
echo "Should mvGenTL_Acquire support U3V devices, such as mvBlueFOX3 (default is 'yes')?"
echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
if [ "$U3V_SUPPORT" == "undefined" ]; then
  if [ "$USE_DEFAULTS" == "NO" ] ; then
    read YES_NO
  else
    YES_NO=""
  fi
  if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
      U3V_SUPPORT=FALSE
  else
      U3V_SUPPORT=TRUE
  fi
fi
echo ""
echo "Should mvGenTL_Acquire support mvBlueNAOS devices (default is 'yes')?"
echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
if [ "$PCIE_SUPPORT" == "undefined" ]; then
  if [ "$USE_DEFAULTS" == "NO" ] ; then
    read YES_NO
  else
    YES_NO=""
  fi
  if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
      PCIE_SUPPORT=FALSE
  else
      PCIE_SUPPORT=TRUE
  fi
fi

if [ "$U3V_SUPPORT" == "FALSE" ] && [ "$GEV_SUPPORT" == "FALSE" ] && [ "$PCIE_SUPPORT" == "FALSE" ]; then
  echo "Warning: As none of the supported technologies provided by this package has been selected" 
  echo "for installation. This means no device will be accessible after the installation has"
  echo "finished."
  let ERROR_NUMBER=ERROR_NUMBER+1
fi

# Create the *.conf files if the system is supporting ld.so.conf.d
if grep -q '/etc/ld.so.conf.d/' /etc/ld.so.conf; then
  $SUDO rm -f $GENICAM_LDSOCONF_FILE; $SUDO touch $GENICAM_LDSOCONF_FILE
  $SUDO rm -f $ACQUIRE_LDSOCONF_FILE; $SUDO touch $ACQUIRE_LDSOCONF_FILE
fi

# Create the export files if the system is supporting profile.d
if grep -q '/etc/profile.d/' /etc/profile; then
  $SUDO rm -f $GENICAM_EXPORT_FILE; $SUDO touch $GENICAM_EXPORT_FILE
  $SUDO rm -f $ACQUIRE_EXPORT_FILE; $SUDO touch $ACQUIRE_EXPORT_FILE
fi

# Check if the destination directory exist, else create it
if ! [ -d $DEF_DIRECTORY ]; then
  # the destination directory does not yet exist
  # first try to create it as a normal user
  mkdir -p $DEF_DIRECTORY >/dev/null 2>&1
  if ! [ -d $DEF_DIRECTORY ]; then
    # that didn't work
    # now try it as superuser
    $SUDO mkdir -p $DEF_DIRECTORY
  fi
  if ! [ -d $DEF_DIRECTORY  ]; then
    echo 'ERROR: Could not create target directory' $DEF_DIRECTORY '.'
    echo 'Problem:'$?
    echo 'Maybe you specified a partition that was mounted read only?'
    echo
    exit
  fi
else
  echo 'Installation directory already exists.'
fi

# in case the directory already existed BUT it belongs to other user
$SUDO chown -R $USER: $DEF_DIRECTORY

# Check the actual tarfile
if ! [ -r $TARFILE ]; then
  echo 'ERROR: could not read' $TARFILE.
  echo
  exit
fi

# needed at compile time (used during development, but not shipped with the final program)
ACT=$API-$VERSION.tar

# needed at run time
BC=mvGenTL_Acquire_runtime
BCT=$BC-$VERSION.tar

# Now unpack the tar-file into /tmp
cd /tmp
tar xfz "$SCRIPTSOURCEDIR/$TARFILE"

# Change to destination directory and remove older libs if any
cd $DEF_DIRECTORY
if ! [ -d $DEF_DIRECTORY/runtime ]; then
  mkdir runtime >/dev/null 2>&1
  if ! [ -d $DEF_DIRECTORY/runtime ]; then
      # that didn't work
      # now try it as superuser
      $SUDO mkdir --parent $DEF_DIRECTORY/runtime
  fi
fi
cd runtime
# Remove older versions (if any)
$SUDO rm -f lib/libmv*.so*

# Now unpack the mvBlueCOUGAR_runtime files
$SUDO tar xf /tmp/$BCT

# The runtime tar contains either the i86 or the x64 tgz
if [ -r GenICam_Runtime_gcc42_Linux32_i86_v3_1_0.tgz ]; then
   if [ x$TARGET != xx86 ]; then
      echo 'Platform conflict : GenICam runtime is 32bit, but target is 64bit'
      let ERROR_NUMBER=ERROR_NUMBER+1
   fi
   $SUDO tar xzf GenICam_Runtime_gcc42_Linux32_i86_v3_1_0.tgz;
   GENVER=`ls GenICam_Runtime_gcc*.tgz | tail -n1 | sed -e s/\\.tgz// | sed -e 's/.\{2\}$//' | cut -dv -f2`
   $SUDO rm GenICam_Runtime_gcc42_Linux32_i86_v3_1_0.tgz
fi
if [ -r GenICam_Runtime_gcc42_Linux64_x64_v3_1_0.tgz ]; then
   if [ x$TARGET = xx86 ]; then
      echo 'Platform conflict : GenICam runtime is 64bit, but target is 32bit'
      let ERROR_NUMBER=ERROR_NUMBER+1
   fi
   $SUDO tar xzf GenICam_Runtime_gcc42_Linux64_x64_v3_1_0.tgz;
   GENVER=`ls GenICam_Runtime_gcc*.tgz | tail -n1 | sed -e s/\\.tgz// | sed -e 's/.\{2\}$//' | cut -dv -f2`
   $SUDO rm GenICam_Runtime_gcc42_Linux64_x64_v3_1_0.tgz
fi

$SUDO chown -R $USER: *

if ! [ -r $GENICAM_EXPORT_FILE ]; then
   echo 'Error : cannot write to' $GENICAM_EXPORT_FILE.
   echo 'After the next boot, the required environment variables will not be set.'
   let ERROR_NUMBER=ERROR_NUMBER+1
   echo
else
   # tests below do not yet check for *commented out* export lines
   if grep -q 'GENICAM_ROOT=' $GENICAM_EXPORT_FILE; then
      echo 'GENICAM_ROOT already defined in' $GENICAM_EXPORT_FILE.
   else
      $SUDO sh -c "echo 'export GENICAM_ROOT=$DEF_DIRECTORY/runtime' >> $GENICAM_EXPORT_FILE"
   fi
   if grep -q 'GENICAM_ROOT_V$GENVER=' $GENICAM_EXPORT_FILE; then
      echo 'GENICAM_ROOT_V'$GENVER' already defined in' $GENICAM_EXPORT_FILE.
   else
      $SUDO sh -c "echo 'export GENICAM_ROOT_V$GENVER=$DEF_DIRECTORY/runtime' >> $GENICAM_EXPORT_FILE"
   fi
   if [ x$TARGET = xx86 ]; then
      if grep -q 'GENICAM_GENTL32_PATH=' $GENICAM_EXPORT_FILE; then
         echo 'GENICAM_GENTL32_PATH already defined in' $GENICAM_EXPORT_FILE.
      else
         $SUDO sh -c "echo 'if [ x\$GENICAM_GENTL32_PATH == x ]; then
   export GENICAM_GENTL32_PATH=$DEF_DIRECTORY/lib/$TARGET
elif [ x\$GENICAM_GENTL32_PATH != x$DEF_DIRECTORY/lib/$TARGET ]; then
   if ! \$(echo \$GENICAM_GENTL32_PATH | grep -q \":$DEF_DIRECTORY/lib/$TARGET\"); then
      export GENICAM_GENTL32_PATH=\$GENICAM_GENTL32_PATH:$DEF_DIRECTORY/lib/$TARGET
   fi
fi' >> $GENICAM_EXPORT_FILE"
      fi
   else
      if grep -q 'GENICAM_GENTL64_PATH=' $GENICAM_EXPORT_FILE; then
         echo 'GENICAM_GENTL64_PATH already defined in' $GENICAM_EXPORT_FILE.
      else
         $SUDO sh -c "echo 'if [ x\$GENICAM_GENTL64_PATH == x ]; then
   export GENICAM_GENTL64_PATH=$DEF_DIRECTORY/lib/$TARGET
elif [ x\$GENICAM_GENTL64_PATH != x$DEF_DIRECTORY/lib/$TARGET ]; then
   if ! \$(echo \$GENICAM_GENTL64_PATH | grep -q \":$DEF_DIRECTORY/lib/$TARGET\"); then
      export GENICAM_GENTL64_PATH=\$GENICAM_GENTL64_PATH:$DEF_DIRECTORY/lib/$TARGET
   fi
fi' >> $GENICAM_EXPORT_FILE"
      fi
   fi

# Since mvIMPACT Acquire version 2.7.0, version 2.4 of the GenICam cache should be able to coexist with
# version 2.3 however they must point to different folders!
# Since mvIMPACT Acquire version 2.14.0, version 3.0 of the GenICam cache should be able to coexist with
# version 2.3 and 2.4 however they must point to different folders!
# Since mvIMPACT Acquire version 2.28.0, version 310 of the GenICam cache should be able to coexist with
# version 2.3, 2.4 and 3.0 however they must point to different folders!
   if grep -q 'GENICAM_CACHE_V2_3=' $GENICAM_EXPORT_FILE; then
      echo 'GENICAM_CACHE_V2_3 already defined in' $GENICAM_EXPORT_FILE.
   fi
   if grep -q 'GENICAM_CACHE_V2_4=' $GENICAM_EXPORT_FILE; then
      echo 'GENICAM_CACHE_V2_4 already defined in' $GENICAM_EXPORT_FILE.
   fi
   if grep -q 'GENICAM_CACHE_V3_0=' $GENICAM_EXPORT_FILE; then
      echo 'GENICAM_CACHE_V3_0 already defined in' $GENICAM_EXPORT_FILE.
   fi
   if grep -q 'GENICAM_CACHE_V3_1=' $GENICAM_EXPORT_FILE; then
      echo 'GENICAM_CACHE_V3_1 already defined in' $GENICAM_EXPORT_FILE.
   else
      $SUDO mkdir -p $DEF_DIRECTORY/runtime/cache/v3_1
      $SUDO sh -c "echo 'export GENICAM_CACHE_V3_1='$DEF_DIRECTORY'/runtime/cache/v3_1' >> $GENICAM_EXPORT_FILE"
   fi
   if grep -q 'GENICAM_LOG_CONFIG_V'$GENVER'=' $GENICAM_EXPORT_FILE; then
      echo 'GENICAM_LOG_CONFIG_V'$GENVER' already defined in' $GENICAM_EXPORT_FILE.
   else
      $SUDO sh -c "echo 'export GENICAM_LOG_CONFIG_V'$GENVER'=$DEF_DIRECTORY/runtime/log/config-unix/DefaultLogging.properties' >> $GENICAM_EXPORT_FILE"
   fi
fi

# Now check if we can unpack the tar file with the device independent stuff
# this is entirely optional
if [ -r /tmp/$ACT ]; then
   cd /tmp
   tar xf /tmp/$ACT
   $SUDO cp -r $ACT2/* $DEF_DIRECTORY
else
  echo
  echo "ERROR: Could not read: /tmp/"$ACT2
  exit
fi

# Set the necessary exports and library paths
cd $DEF_DIRECTORY
if grep -q 'MVIMPACT_ACQUIRE_DIR=' $ACQUIRE_EXPORT_FILE; then
   echo 'MVIMPACT_ACQUIRE_DIR already defined in' $ACQUIRE_EXPORT_FILE.
else
   $SUDO sh -c "echo 'export MVIMPACT_ACQUIRE_DIR=$DEF_DIRECTORY' >> $ACQUIRE_EXPORT_FILE"
fi

if grep -q "$DEF_DIRECTORY/lib/$TARGET" $ACQUIRE_LDSOCONF_FILE; then
   echo "$DEF_DIRECTORY/lib/$TARGET already defined in" $ACQUIRE_LDSOCONF_FILE.
else
   $SUDO sh -c "echo '$DEF_DIRECTORY/lib/$TARGET' >> $ACQUIRE_LDSOCONF_FILE"
fi
if grep -q "$DEF_DIRECTORY/Toolkits/expat/bin/$TARGET/lib" $ACQUIRE_LDSOCONF_FILE; then
   echo "$DEF_DIRECTORY/Toolkits/expat/bin/$TARGET/lib already defined in" $ACQUIRE_LDSOCONF_FILE.
else
   $SUDO sh -c "echo '$DEF_DIRECTORY/Toolkits/expat/bin/$TARGET/lib' >> $ACQUIRE_LDSOCONF_FILE"
fi

# Now do the shared linker setup
if ! [ -r $GENICAM_LDSOCONF_FILE ]; then
   echo 'Error : cannot write to' $GENICAM_LDSOCONF_FILE.
   echo 'Execution will fail, as at run time, the shared objects will not be found.'
   let ERROR_NUMBER=ERROR_NUMBER+1
   echo
else
   if [ x$TARGET = xx86 ]; then
      GENILIBPATH=Linux32_i86
   else
      GENILIBPATH=Linux64_x64
   fi
   # tests below do not check for *commented out* link lines
   # must later add sub-string check
   # GenICam libs
   if grep -q "$DEF_DIRECTORY/runtime/bin/$GENILIBPATH" $GENICAM_LDSOCONF_FILE; then
      echo "$DEF_DIRECTORY/runtime/bin/$GENILIBPATH already defined in" $GENICAM_LDSOCONF_FILE.
   else
      $SUDO sh -c "echo '$DEF_DIRECTORY/runtime/bin/$GENILIBPATH' >> $GENICAM_LDSOCONF_FILE"
   fi
fi

# This variable must be exported, or else wxPropView-related make problems can arise
export MVIMPACT_ACQUIRE_DIR=$DEF_DIRECTORY

# Move all the mvIMPACT Acquire related libraries to the mvIA/lib folder.
if [ -r /tmp/$ACT ]; then
   cd $DEF_DIRECTORY/lib/$TARGET
   $SUDO mv $DEF_DIRECTORY/runtime/lib/* .
   $SUDO rmdir $DEF_DIRECTORY/runtime/lib
fi

# Clean up /tmp
rm -r -f /tmp/$ACT /tmp/$BCT /tmp/$API-$VERSION

# create softlinks for the Toolkits libraries
createSoftlink $DEF_DIRECTORY/Toolkits/expat/bin/$TARGET/lib libexpat.so.0.5.0 libexpat.so.0
createSoftlink $DEF_DIRECTORY/Toolkits/expat/bin/$TARGET/lib libexpat.so.0 libexpat.so
createSoftlink $DEF_DIRECTORY/Toolkits/FreeImage3160/bin/Release/FreeImage/$TARGET libfreeimage-3.16.0.so libfreeimage.so.3
createSoftlink $DEF_DIRECTORY/Toolkits/FreeImage3160/bin/Release/FreeImage/$TARGET libfreeimage.so.3 libfreeimage.so
if [ "$U3V_SUPPORT" == "TRUE" ]; then
   createSoftlink $DEF_DIRECTORY/Toolkits/libusb-1.0.21/bin/$TARGET/lib libusb-1.0.so.0.1.0 libusb-1.0.so.0
   createSoftlink $DEF_DIRECTORY/Toolkits/libusb-1.0.21/bin/$TARGET/lib libusb-1.0.so.0 libusb-1.0.so
   createSoftlink $DEF_DIRECTORY/Toolkits/libudev/bin/$TARGET/lib libudev.so.0.13.0 libudev.so.0
   createSoftlink $DEF_DIRECTORY/Toolkits/libudev/bin/$TARGET/lib libudev.so.0 libudev.so
else
   $SUDO rm -rf $DEF_DIRECTORY/Toolkits/libusb-1.0.21 >/dev/null 2>&1
   $SUDO rm -rf $DEF_DIRECTORY/Toolkits/libudev >/dev/null 2>&1
fi

# Update the library cache with ldconfig
$SUDO /sbin/ldconfig

if [ "$MINIMAL_INSTALLATION" == "NO" ] ; then
  # apt-get extra parameters
  if [ "$USE_DEFAULTS" == "YES" ] ; then
    APT_GET_EXTRA_PARAMS=" -y"
    YUM_EXTRA_PARAMS=" -yq"
  fi
  
  # Install needed libraries and compiler
  COULD_NOT_INSTALL="Could not find the package manager of the system; please install >%s< manually."
  DISTRO_NOT_SUPPORTED="Your Linux distribution is currently not officially supported by this installation script. Please install required package >%s< manually."
  
  # Check if we have g++
  if ! which g++ >/dev/null 2>&1; then
    if [ "$OS_NAME" == "Ubuntu" ] || [ "$OS_NAME" == "Debian" ]; then
      if which apt-get >/dev/null 2>&1; then
        $SUDO apt-get $APT_GET_EXTRA_PARAMS -q install g++
      fi
     elif [ "$OS_NAME" == "SuSE" ] ; then
      if $SUDO which yast >/dev/null 2>&1; then
        YASTBIN=`$SUDO which yast`
        $SUDO $YASTBIN --install gcc-c++ make
      else
        printf "$COULD_NOT_INSTALL" "g++"
        let WARNING_NUMBER=WARNING_NUMBER+1
      fi
    elif [ "$OS_NAME" == "RedHat" ] || [ "$OS_NAME" == "Fedora" ] ; then
      if $SUDO which yum >/dev/null 2>&1; then
        echo 'Installing gcc and gcc-c++'
        $SUDO yum $YUM_EXTRA_PARAMS install gcc gcc-c++
      else
        printf "$COULD_NOT_INSTALL" "g++"
        let WARNING_NUMBER=WARNING_NUMBER+1
      fi
    else
      printf "$DISTRO_NOT_SUPPORTED" "g++"
      let WARNING_NUMBER=WARNING_NUMBER+1
    fi
  fi
  
  INPUT_REQUEST="Do you want to install >%s< (default is 'yes')?\nHit 'n' + <Enter> for 'no', or just <Enter> for 'yes'.\n"
  YES_NO=
  
  # Check if we have expat-devel   
  if [ "$OS_NAME" == "Ubuntu" ] || [ "$OS_NAME" == "Debian" ]; then
    if which apt-get >/dev/null 2>&1; then
      $SUDO apt-get $APT_GET_EXTRA_PARAMS -q install libexpat1-dev
    fi
  elif [ "$OS_NAME" == "SuSE" ] ; then
    if $SUDO which yast >/dev/null 2>&1; then
      YASTBIN=`$SUDO which yast`
      $SUDO $YASTBIN --install expat libexpat-devel
    fi
  elif [ "$OS_NAME" == "RedHat" ] || [ "$OS_NAME" == "Fedora" ] ; then
      if $SUDO which yum >/dev/null 2>&1; then
        $SUDO yum $YUM_EXTRA_PARAMS install expat-devel
      fi
     else
    printf "$COULD_NOT_INSTALL" "libexpat"
    let WARNING_NUMBER=WARNING_NUMBER+1
  fi
  
  # Do we want to install wxWidgets?
  if [ "$(wx-config --release 2>&1 | grep -c "^3.")" != "1" ]; then
     echo
     printf "$INPUT_REQUEST" "wxWidgets"
     echo "This is highly recommended, as without wxWidgets, you cannot build wxPropView."
     echo
     if [ "$USE_DEFAULTS" == "NO" ] ; then
       read YES_NO
     else
       YES_NO=""
     fi
     if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
        echo 'Not installing wxWidgets'
     else
      if [ "$OS_NAME" == "Ubuntu" ] || [ "$OS_NAME" == "Debian" ]; then
        if which apt-get >/dev/null 2>&1; then
          $SUDO apt-get update
          echo 'Installing wxWidgets'
          VERSION="$(cut -d'.' -f1 <<< $OS_VERSION)"
          if [ "$OS_NAME" == "Ubuntu" ] && [ "$VERSION" -ge "20" ]; then
              $SUDO apt-get $APT_GET_EXTRA_PARAMS -q install libwxgtk3.0-gtk3-* libwxgtk3.0-gtk3-dev libwxbase3.0-0* libwxbase3.0-dev wx3.0-headers build-essential libgtk2.0-dev
          else
              $SUDO apt-get $APT_GET_EXTRA_PARAMS -q install libwxgtk3.0-dev libwxbase3.0-0* libwxbase3.0-dev libwxgtk3.0-0* wx3.0-headers build-essential libgtk2.0-dev
          fi
        else
          printf "$COULD_NOT_INSTALL" "wxWidgets"
          let WARNING_NUMBER=WARNING_NUMBER+1
        fi
      elif [ "$OS_NAME" == "SuSE" ] ; then
        if $SUDO which yast >/dev/null 2>&1; then
          echo 'Installing wxWidgets'
          YASTBIN=`$SUDO which yast`
          $SUDO $YASTBIN --install wxGTK-devel
        else
          printf "$COULD_NOT_INSTALL" "wxWidgets"
          let WARNING_NUMBER=WARNING_NUMBER+1
        fi
      elif [ "$OS_NAME" == "RedHat" ] || [ "$OS_NAME" == "Fedora" ] ; then
        if $SUDO which yum >/dev/null 2>&1; then
          echo 'Installing wxWidgets'
          $SUDO yum $YUM_EXTRA_PARAMS install wxGTK3 wxGTK3-devel
          printf "$COULD_NOT_INSTALL" "wxWidgets"
          let WARNING_NUMBER=WARNING_NUMBER+1
        else
          printf "$DISTRO_NOT_SUPPORTED" "wxWidgets"
          let WARNING_NUMBER=WARNING_NUMBER+1
        fi
      fi
    fi
  fi
  
  # Do we want to install FLTK?
  if ! which fltk-config >/dev/null 2>&1; then
     echo
     printf "$INPUT_REQUEST" "FLTK"
     echo "This is only required if you want to build the 'ContinuousCaptureFLTK' sample."
     echo
     if [ "$USE_DEFAULTS" == "NO" ] ; then
       read YES_NO
     else
       YES_NO=""
     fi
     if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
        echo 'Not installing FLTK'
     else
       if [ "$OS_NAME" == "Ubuntu" ] || [ "$OS_NAME" == "Debian" ]; then
        if which apt-get >/dev/null 2>&1; then
          echo 'Installing FLTK'
          $SUDO apt-get $APT_GET_EXTRA_PARAMS -q install libgl1-mesa-dev
          $SUDO apt-get $APT_GET_EXTRA_PARAMS -q install libfltk1.1-dev
        else
          printf "$COULD_NOT_INSTALL" "FLTK"
          let WARNING_NUMBER=WARNING_NUMBER+1
        fi
       elif [ "$OS_NAME" == "SuSE" ] ; then
         if $SUDO which yast >/dev/null 2>&1; then
          echo 'Installing FLTK'
          YASTBIN=`$SUDO which yast`
          $SUDO $YASTBIN --install Mesa-devel
          $SUDO $YASTBIN --install fltk-devel
         else
          printf "$COULD_NOT_INSTALL" "FLTK"
          let WARNING_NUMBER=WARNING_NUMBER+1
        fi
       elif [ "$OS_NAME" == "RedHat" ] || [ "$OS_NAME" == "Fedora" ] ; then
        if $SUDO which yum >/dev/null 2>&1; then
         echo 'Installing FLTK'
         $SUDO yum $YUM_EXTRA_PARAMS install fltk fltk-devel
        else
          printf "$COULD_NOT_INSTALL" "FLTK"
          let WARNING_NUMBER=WARNING_NUMBER+1
        fi
       else
        printf "$DISTRO_NOT_SUPPORTED" "FLTK"
        let WARNING_NUMBER=WARNING_NUMBER+1
       fi
     fi
  fi
  
  # In case GEV devices should not be supported remove mvIPConfigure 
  if [ "$GEV_SUPPORT" == "FALSE" ]; then
    if [ -d $DEF_DIRECTORY/apps/mvIPConfigure ] && [ -r $DEF_DIRECTORY/apps/mvIPConfigure/Makefile ]; then
      $SUDO rm -rf $DEF_DIRECTORY/apps/mvIPConfigure >/dev/null 2>&1
    fi
  fi
  
  echo
  echo "Do you want the tools and samples to be built (default is 'yes')?"
  echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
  if [ "$USE_DEFAULTS" == "NO" ] ; then
    read YES_NO
  else
    YES_NO=""
  fi
  if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
     echo
     echo "The tools and samples were not built."
     echo "To build them manually run 'make native' in $DEF_DIRECTORY"
  else
     cd $DEF_DIRECTORY
     $SUDO chown -R $USER: $DEF_DIRECTORY
     make $TARGET
     if [ $? -ne 0 ]; then
        let WARNING_NUMBER=WARNING_NUMBER+1
     fi
  
  # Shall the MV Tools be linked in /usr/bin?
     if [ "$GEV_SUPPORT" == "TRUE" ]; then
         echo "Do you want to set a link to /usr/bin for wxPropView, mvIPConfigure and mvDeviceConfigure (default is 'yes')?"
     else
         echo "Do you want to set a link to /usr/bin for wxPropView and mvDeviceConfigure (default is 'yes')?"
     fi
     echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
     if [ "$USE_DEFAULTS" == "NO" ] ; then
       read YES_NO
     else
       YES_NO=""
     fi
     if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
        echo "Will not set any new link to /usr/bin."
     else
        if [ -r /usr/bin ]; then
           # Set wxPropView
           if [ -r $DEF_DIRECTORY/apps/mvPropView/$TARGET/wxPropView ]; then
              $SUDO rm -f /usr/bin/wxPropView
              $SUDO ln -s $DEF_DIRECTORY/apps/mvPropView/$TARGET/wxPropView /usr/bin/wxPropView
           fi
           # Set mvIPConfigure
           if [ "$GEV_SUPPORT" == "TRUE" ]; then
               if [ -r $DEF_DIRECTORY/apps/mvIPConfigure/$TARGET/mvIPConfigure ]; then
                  $SUDO rm -f /usr/bin/mvIPConfigure
                  $SUDO ln -s $DEF_DIRECTORY/apps/mvIPConfigure/$TARGET/mvIPConfigure /usr/bin/mvIPConfigure
               fi
           fi
           # Set mvDeviceConfigure
           if [ -r $DEF_DIRECTORY/apps/mvDeviceConfigure/$TARGET/mvDeviceConfigure ]; then
              $SUDO rm -f /usr/bin/mvDeviceConfigure
              $SUDO ln -s $DEF_DIRECTORY/apps/mvDeviceConfigure/$TARGET/mvDeviceConfigure /usr/bin/mvDeviceConfigure
           fi
        fi
     fi
    
    # Should wxPropView check weekly for updates?
    echo "Do you want wxPropView to check for updates weekly(default is 'yes')?"
    echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
    if [ "$USE_DEFAULTS" == "NO" ] ; then
      read YES_NO
    else
      YES_NO=""
    fi
    if [ ! -e ~/.wxPropView ]; then
      touch ~/.wxPropView
    fi
    if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
      if [ "$(grep -c AutoCheckForUpdatesWeekly ~/.wxPropView)" -ne "0" ]; then
        Tweakline=$(( $( grep -n "AutoCheckForUpdatesWeekly" ~/.wxPropView | cut -d: -f1) )) && sed -i "$Tweakline s/.*/AutoCheckForUpdatesWeekly=0/" ~/.wxPropView
      else
        echo "AutoCheckForUpdatesWeekly=0" >> ~/.wxPropView
      fi
    else
      if [ "$(grep -c AutoCheckForUpdatesWeekly ~/.wxPropView)" -ne "0" ]; then
        Tweakline=$(( $( grep -n "AutoCheckForUpdatesWeekly" ~/.wxPropView | cut -d: -f1) )) && sed -i "$Tweakline s/.*/AutoCheckForUpdatesWeekly=1/" ~/.wxPropView
      else
        echo "[MainFrame/Help]" >> ~/.wxPropView
        echo "AutoCheckForUpdatesWeekly=1" >> ~/.wxPropView
      fi
    fi
  fi
fi

# building kernel module for mvBlueNAOS
if [ "$PCIE_SUPPORT" == "TRUE" ]; then
    echo
    echo "Do you want the mvBlueNAOS kernel module to be built (default is 'yes')?"
    echo "The kernel module MUST be built for mvBlueNAOS cameras to be usable!"
    echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
    if [ "$USE_DEFAULTS" == "NO" ] ; then
      read YES_NO
    else
      YES_NO=""
    fi
    if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
      echo
      echo "Please restart the installer and select 'YES' if you want to use a mvBlueNAOS camera!"
      let ERROR_NUMBER=ERROR_NUMBER+1
      echo
    else
      if [ -r $DEF_DIRECTORY ]; then
        BN_INSTALLED="FALSE"
        KM_SRC_DIR=$DEF_DIRECTORY/kernelmodules/linux/mvBlueNAOS
        BN_MODNAME="mvpci"
        DKMS=$($SUDO which dkms)

        # unload any older version of the kernel module
        MOD_STATUS=$(lsmod | grep -w $BN_MODNAME)
        if [ "x${MOD_STATUS}" != "x" ]; then
          $SUDO rmmod $BN_MODNAME
        fi
        # delete any older version of the kernel module that may have been installed without dkms
        $SUDO rm -f /lib/modules/$(uname -r)/kernel/misc/${BN_MODNAME}.ko

        # try to install using dkms if it is available
        if [ "x${DKMS}" != "x" ]; then
          BN_VERSION=$(grep "PACKAGE_VERSION" $KM_SRC_DIR/dkms.conf | awk -F '=' '{print $2}')
          # if exactly this version is registered with dkms then remove it
          STATUS=$($DKMS status $BN_MODNAME/$BN_VERSION | grep "installed")
          if [ "x${STATUS}" != "x" ]; then
            echo "The kernel module $BN_MODNAME/$BN_VERSION was already installed using dkms. Updating and rebuilding it for the current kernel."
            $SUDO $DKMS remove $BN_MODNAME/$BN_VERSION -k $(uname -r) -q
          fi

          echo "Adding, building and installing the Kernel module $BN_MODNAME/$BN_VERSION using dkms."
          $SUDO $DKMS install $BN_MODNAME/$BN_VERSION
          STATUS=$($DKMS status $BN_MODNAME/$BN_VERSION | grep "installed")
          if [ "x${STATUS}" != "x" ]; then
            echo "Kernel module $BN_MODNAME/$BN_VERSION installed using dkms. It will be automatically rebuilt if you update the kernel."
            BN_INSTALLED="TRUE"
          fi
        else
          echo "dkms not installed on this system! You will have to rebuild the kernel module $BN_MODNAME if you update the kernel."
        fi

        if [ $BN_INSTALLED != "TRUE" ]; then
          # fall back to compiling manually if dkms not possible
          echo "**********************************************************"
          echo ""
          echo "               BUILDING mvBlueNAOS kernel module..."
          echo ""
          echo "**********************************************************"
          cd $KM_SRC_DIR
          $SUDO make install
          if [ $? -ne 0 ]; then
            let ERROR_NUMBER=ERROR_NUMBER+1
          else
            BN_INSTALLED="TRUE"
          fi
        fi
      fi

      # load the new kernel module
      if [ $BN_INSTALLED = "TRUE" ]; then
        $SUDO modprobe $BN_MODNAME
      fi
    fi
fi

# Update the library cache again.
$SUDO /sbin/ldconfig

# copy the mvBF3 boot-device and an universal udev rules file for U3V cameras to the system 
if [ "$U3V_SUPPORT" == "TRUE" ]; then
    echo
    echo "Do you want to copy the necessary files to /etc/udev/rules.d for U3V device support (default is 'yes')?"
    echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
    if [ "$USE_DEFAULTS" == "NO" ] ; then
      read YES_NO
    else
      YES_NO=""
    fi
    if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
       echo
       echo 'To be able to use U3V devices, copy 52-U3V.rules and 52-mvbf3.rules the file to /etc/udev/rules.d'
       echo
    else
       $SUDO cp -f $DEF_DIRECTORY/Scripts/52-U3V.rules /etc/udev/rules.d
       $SUDO cp -f $DEF_DIRECTORY/Scripts/52-mvbf3.rules /etc/udev/rules.d
    fi
fi

# install the mvBlueNAOS universal udev rules file and scripts
if [ "$PCIE_SUPPORT" == "TRUE" ]; then
    echo
    echo "Do you want to copy the necessary files to /etc/udev/rules.d for mvBlueNAOS device support (default is 'yes')?"
    echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
    if [ "$USE_DEFAULTS" == "NO" ] ; then
      read YES_NO
    else
      YES_NO=""
    fi
    if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
       echo
       echo 'To be able to use mvBlueNAOS devices, copy 51-udev-pcie.rules to /etc/udev/rules.d'
       echo 'and make_device.sh and device_namer.sh to /usr/local/bin/.'
    else
       $SUDO cp -f $DEF_DIRECTORY/Scripts/51-udev-pcie.rules /etc/udev/rules.d
       $SUDO cp -f $DEF_DIRECTORY/Scripts/make_device.sh /usr/local/bin/
       $SUDO cp -f $DEF_DIRECTORY/Scripts/device_namer.sh /usr/local/bin/
    fi
fi

# check if plugdev group exists and the user is member of it
if [ "$(grep -c ^plugdev: /etc/group )" == "0" ]; then
  echo "Group 'plugdev' doesn't exist, this is necessary to use U3V devices as a normal user,"
  echo "do you want to create it and add current user to 'plugdev' (default is 'yes')?"
  echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
  if [ "$USE_DEFAULTS" == "NO" ] ; then
    read YES_NO
  else
    YES_NO=""
  fi
  if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
    echo
    echo "'plugdev' will not be created and you can't run the device as non-root user!"
    echo "If you want non-root users support, you will need to create 'plugdev'"
    echo "and add the users to this group."
    let WARNING_NUMBER=WARNING_NUMBER+1
 else
    $SUDO /usr/sbin/groupadd -g 46 plugdev
    $SUDO /usr/sbin/usermod -a -G plugdev $USER
    echo "Group 'plugdev' created and user '"$USER"' added to it."
  fi
else
  if [ "$( groups | grep -c plugdev )" == "0" ]; then
    echo "Group 'plugdev' exists, however user '"$USER"' is not a member, which is necessary to"
    echo "use U3V devices. Do you want to add  user '"$USER"' to 'plugdev' (default is 'yes')?"
    echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
    if [ "$USE_DEFAULTS" == "NO" ] ; then
      read YES_NO
    else
      YES_NO=""
    fi
    if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
      echo
      echo "If you want to use U3V devices you have to manually add user '"$USER"' to the plugdev group."
      let WARNING_NUMBER=WARNING_NUMBER+1
    else
      $SUDO /usr/sbin/usermod -a -G plugdev $USER
      echo "User '"$USER"' added to 'plugdev' group."
    fi
  fi
fi
echo

# create the cache for genicam xml files.
if ! [ -d $DEF_DATA_DIRECTORY/genicam ]; then
  mkdir -p $DEF_DATA_DIRECTORY/genicam >/dev/null 2>&1
  if ! [ -d $DEF_DATA_DIRECTORY/genicam ]; then
      # that didn't work, now try it as superuser
      $SUDO mkdir -p $DEF_DATA_DIRECTORY/genicam >/dev/null 2>&1
  fi
  if ! [ -d $DEF_DATA_DIRECTORY/genicam ]; then
    echo "ERROR: Could not create " $DEF_DATA_DIRECTORY/genicam " directory."
    echo 'Problem:'$?
    echo 'Maybe you specified a partition that was mounted read only?'
    echo
    exit
  fi
fi

# create the logs directory and set MVIMPACT_ACQUIRE_DATA_DIR.
if ! [ -d $DEF_DATA_DIRECTORY/logs ]; then
  mkdir -p $DEF_DATA_DIRECTORY/logs >/dev/null 2>&1
  if ! [ -d $DEF_DATA_DIRECTORY/logs ]; then
      # that didn't work, now try it as superuser
      $SUDO mkdir -p $DEF_DATA_DIRECTORY/logs >/dev/null 2>&1
  fi
fi

if [ -d $DEF_DATA_DIRECTORY/logs ]; then
  # exec (1) and write (2) are needed to create new files and reading (4) should be allowed anyway
  # therefore the permissions are set to 777, so any user is able to read and write logs
  $SUDO chmod 777 $DEF_DATA_DIRECTORY/logs
  mv $DEF_DIRECTORY/apps/mvDebugFlags.mvd $DEF_DATA_DIRECTORY/logs >/dev/null 2>&1
  if ! [ -r $DEF_DATA_DIRECTORY/logs/mvDebugFlags.mvd ]; then
    $SUDO mv $DEF_DIRECTORY/apps/mvDebugFlags.mvd $DEF_DATA_DIRECTORY/logs >/dev/null 2>&1
  fi
  if grep -q 'MVIMPACT_ACQUIRE_DATA_DIR=' $ACQUIRE_EXPORT_FILE; then
    echo 'MVIMPACT_ACQUIRE_DATA_DIR already defined in' $ACQUIRE_EXPORT_FILE.
  else
    $SUDO sh -c "echo 'export MVIMPACT_ACQUIRE_DATA_DIR=$DEF_DATA_DIRECTORY' >> $ACQUIRE_EXPORT_FILE"
  fi
else
  echo "ERROR: Could not create " $DEF_DATA_DIRECTORY/logs " directory."
  echo 'Problem:'$?
  echo 'Maybe you specified a partition that was mounted read only?'
  echo
  exit
fi

# make sure the complete mvIA-tree and the data folder belongs to the user
$SUDO chown -R $USER: $DEF_DIRECTORY
$SUDO chown -R $USER: $DEF_DATA_DIRECTORY

# Create the ignoredInterfaces.txt file taking settings persistency into account
if [ -r $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.txt ]; then 
  rm -f $DEF_DIRECTORY/ignoredInterfaces.txt >/dev/null 2>&1
else
  mv $DEF_DIRECTORY/ignoredInterfaces.txt $DEF_DATA_DIRECTORY/logs >/dev/null 2>&1
  if ! [ -r $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.txt ]; then
    $SUDO mv $DEF_DIRECTORY/ignoredInterfaces.txt $DEF_DATA_DIRECTORY/logs >/dev/null 2>&1
  fi
fi

# Configure the ignoredInterfaces.txt file according to the user preferences
if [ -r $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.txt ]; then 
  if [ "$GEV_SUPPORT" == "TRUE" ]; then
    sed -i '/GEV=I/d' $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.txt
  else
    if [ "$(grep -c 'GEV=' $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.txt )" == "0" ]; then
      echo "GEV=I" >> $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.txt
    else
      sed -i "s/GEV=./GEV=I/" $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.txt
    fi
  fi
  if [ "$U3V_SUPPORT" == "TRUE" ]; then
    sed -i '/U3V=I/d' $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.txt
  else
    if [ "$(grep -c 'U3V=' $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.txt )" == "0" ]; then
      echo "U3V=I" >> $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.txt
    else
      sed -i "s/U3V=./U3V=I/" $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.txt
    fi
  fi
fi

# Create the ignoredInterfaces.pcie.txt file taking settings persistency into account
if [ -r $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.pcie.txt ]; then 
  rm -f $DEF_DIRECTORY/ignoredInterfaces.pcie.txt >/dev/null 2>&1
else
  mv $DEF_DIRECTORY/ignoredInterfaces.pcie.txt $DEF_DATA_DIRECTORY/logs >/dev/null 2>&1
  if ! [ -r $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.pcie.txt ]; then
    $SUDO mv $DEF_DIRECTORY/ignoredInterfaces.pcie.txt $DEF_DATA_DIRECTORY/logs >/dev/null 2>&1
  fi
fi

# Configure the ignoredInterfaces.pcie.txt file according to the user preferences
if [ -r $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.pcie.txt ]; then 
  if [ "$PCIE_SUPPORT" == "TRUE" ]; then
    sed -i '/PCI=I/d' $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.pcie.txt
  else
    if [ "$(grep -c 'PCI=' $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.pcie.txt )" == "0" ]; then
      echo "PCI=I" >> $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.pcie.txt
    else
      sed -i "s/PCI=./PCI=I/" $DEF_DATA_DIRECTORY/logs/ignoredInterfaces.pcie.txt
    fi
  fi
fi

# Configure the /etc/security/limits.d/acquire.conf file to be able to set thread priorities
if [ -d /etc/security/limits.d ]; then
  if [[ ! -f /etc/security/limits.d/acquire.conf || "$(grep -c '@plugdev            -       nice            -20' /etc/security/limits.d/acquire.conf )" == "0" ]] ; then
    echo '@plugdev            -       nice            -20' | sudo tee -a /etc/security/limits.d/acquire.conf >/dev/null
  fi
  if [ "$(grep -c '@plugdev            -       rtprio          99' /etc/security/limits.d/acquire.conf )" == "0" ] ; then
    echo '@plugdev            -       rtprio          99' | sudo tee -a /etc/security/limits.d/acquire.conf >/dev/null
  fi
else
  echo 'INFO: Directory /etc/security/limits.d is missing, mvIMPACT Acquire will not'
  echo 'be able to set thread priorities correctly. Incomplete frames may occur!!!'
  let WARNING_NUMBER=WARNING_NUMBER+1
fi

# Set the necessary cti softlinks
if [ "$GEV_SUPPORT" == "TRUE" ] || [ "$U3V_SUPPORT" == "TRUE" ]; then
  createSoftlink $DEF_DIRECTORY/lib/$TARGET libmvGenTLProducer.so mvGenTLProducer.cti
fi
if [ "$PCIE_SUPPORT" == "TRUE" ]; then
  createSoftlink $DEF_DIRECTORY/lib/$TARGET libmvGenTLProducer.PCIe.so mvGenTLProducer.PCIe.cti
fi

if [ "$MINIMAL_INSTALLATION" == "NO" ] ; then
  # A bunch of actions necessary if GEV support is enabled
  if [ "$GEV_SUPPORT" == "TRUE" ]; then
    # Ensure the necessary arping capabilities are set.
    $SUDO setcap cap_net_raw+ep $(which arping)>/dev/null 2>&1
    
    # Ensure the necessary capabilities for mv applications are set.
    $SUDO setcap cap_net_bind_service,cap_net_raw+ep $DEF_DIRECTORY/apps/mvPropView/$TARGET/wxPropView
    $SUDO setcap cap_net_bind_service,cap_net_raw+ep $DEF_DIRECTORY/apps/mvIPConfigure/$TARGET/mvIPConfigure
    $SUDO setcap cap_net_bind_service,cap_net_raw+ep $DEF_DIRECTORY/apps/mvDeviceConfigure/$TARGET/mvDeviceConfigure
    
    # Increase the network buffers to prevent incomplete frames
    echo 'net.core.wmem_max=4194304' > /tmp/62-buffers-performance.conf
    echo 'net.core.rmem_max=16777216' >> /tmp/62-buffers-performance.conf
    echo 'net.core.wmem_default=4194304' >> /tmp/62-buffers-performance.conf
    echo 'net.core.rmem_default=16777216' >> /tmp/62-buffers-performance.conf
    echo 'net.core.netdev_max_backlog=10000' >> /tmp/62-buffers-performance.conf
    
    # Fine-tune reverse-path-filtering to allow for discovery of GEV cameras with bad network configurations.
    echo 'net.ipv4.conf.all.rp_filter = 2' >> /tmp/62-buffers-performance.conf
    echo 'net.ipv4.conf.default.rp_filter = 2' >> /tmp/62-buffers-performance.conf
    
    $SUDO mv /tmp/62-buffers-performance.conf /etc/sysctl.d/
    $SUDO sysctl -p /etc/sysctl.d/62-buffers-performance.conf >/dev/null 2>&1
  fi

  # Check if a systemd service should handle usbcore.usbfs_memory_mb settings
  if [ "$U3V_SUPPORT" == "TRUE" ]; then
    if [ $INIT_SYSTEM == "systemd" ]; then
      echo
      echo "Since this system uses systemd as init-system, a systemd service could be created now that would automatically"
      echo "invoke the settings from file 'usbfs_memory_mb' at boot time which would then be determined by this script."
      echo
      echo "The size of this memory corresponds to the amount of memory that can be used by all USB3 Vision cameras as"
      echo "well as all other USB devices in the system. Usually USB3 Vision devices use most if not all of this memory"
      echo "for queuing capture buffers thus more memory will allow more buffers/cameras to be used in parallel!"
      echo
      echo "For more details regarding usbcore.usbfs_memory_mb settings, including sample calculations, please visit:"
      echo "https://www.matrix-vision.com/manuals/mvBlueFOX3/index.html"
      echo " -> Troubleshooting"
      echo "  -> USB3.0 Issues"
      echo "   -> How can I improve the USB 3.0 enviroment?"
      echo "    -> Checklist for Linux"
      echo "     -> Kernel memory"
      echo
      echo "Would you like to proceed like this (default is 'yes')?"
      echo
      echo "Selecting 'yes' will cause this script to create a systemd-service which will configure the usbfs_memory_mb"
      echo "parameter to make sure the USB3 Vision cameras work as expected."
      echo
      echo "Selecting 'no' will make it necessary to configure the value manually as it is mandatory for USB3 Vision"
      echo "devices to work without image transfer issues."
      echo
      echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
      if [ "$USE_DEFAULTS" == "NO" ] ; then
        read YES_NO
      else
        YES_NO=""
      fi
      if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
        SYSTEMS_USBFS_MEMORY_MB_VALUE=$(cat /sys/module/usbcore/parameters/usbfs_memory_mb)
        SYSTEMS_MEMORY_TOTAL_KB=$(cat /proc/meminfo | grep MemTotal: | sed 's/[^0-9]*//g')
        ((SYSTEMS_MEMORY_TOTAL_MB=$SYSTEMS_MEMORY_TOTAL_KB/1024))
        echo
        echo "How much memory should be reserved for USB traffic (OS default is typically 16MB which"
        echo "is by far not enough to operate USB3 Vision cameras with high bandwidth usage. A value"
        echo "of around 256MB is usually a better choice)?"
        echo
        echo "It will be important to select a good compromise between the amount of data which is expected"
        echo "and the total memory which is available."
        echo 
        echo "Insufficient memory will lead to image transmission issues and in some cases even to issues"
        echo "when configuring devices!"
        echo 
        echo "Too much memory might cause a huge amount of unused memory reserved for USB traffic that could"
        echo "otherwise be used by the operating system for other tasks."
        echo
        echo "PROVIDING MORE MEMORY THAN NECESSARY DOES NOT HAVE ANY POSITIVE IMPACT!"
        echo
        echo "For more details regarding usbcore.usbfs_memory_mb settings, including sample calculations, please visit:"
        echo "https://www.matrix-vision.com/manuals/mvBlueFOX3/index.html"
        echo " -> Troubleshooting"
        echo "  -> USB3.0 Issues"
        echo "   -> How can I improve the USB 3.0 enviroment?"
        echo "    -> Checklist for Linux"
        echo "     -> Kernel memory"
        echo
        echo "Currently configured: ${SYSTEMS_USBFS_MEMORY_MB_VALUE} MB"
        echo "Total available RAM:  ${SYSTEMS_MEMORY_TOTAL_MB} MB"
        echo
        echo "Please specify the amount of memory (in Megabyte) which should be reserved for USB data"
        read USER_SELECTED_MEMORY
        if [ "$SYSTEMS_MEMORY_TOTAL_MB" -gt "$USER_SELECTED_MEMORY" ]; then
            USBFS_MEMORY_MB_VALUE=$USER_SELECTED_MEMORY
        elif [ $? -ne 0 ]; then
            USBFS_MEMORY_MB_VALUE=256 
            echo "Error! Invalid value passed. Will use default value: ${USBFS_MEMORY_MB_VALUE} MB"
        else
            USBFS_MEMORY_MB_VALUE=256 
            echo "Error! Specified memory is more than the system's total memory!"
            echo "Configuring to the default value of: ${USBFS_MEMORY_MB_VALUE} MB"
        fi
      else
        USBFS_MEMORY_MB_VALUE=256
      fi
      echo "Configuring usbcore.usbfs_memory_mb automatically to use: ${USBFS_MEMORY_MB_VALUE} MB for"
      echo "USB traffic"
      USBFS_CONFIGURED_PER_SYSTEMD="YES"
      echo -e "#!/bin/bash
USBFS_MEMORY_MB_VALUE=$USBFS_MEMORY_MB_VALUE
SYSTEMS_USBFS_MEMORY_MB_VALUE=\$(cat /sys/module/usbcore/parameters/usbfs_memory_mb)
echo \"usbcore.usbfs_memory_mb:  \${SYSTEMS_USBFS_MEMORY_MB_VALUE}\" | systemd-cat -p info
echo \"Changing to: \${USBFS_MEMORY_MB_VALUE} MB\"
sh -c \"echo \${USBFS_MEMORY_MB_VALUE} > /sys/module/usbcore/parameters/usbfs_memory_mb\"

SYSTEMS_USBFS_MEMORY_MB_VALUE=\$(cat /sys/module/usbcore/parameters/usbfs_memory_mb)
if [ $? -eq 0 ]; then
echo \"usbcore.usbfs_memory_mb: Changed sucessfully to: \${SYSTEMS_USBFS_MEMORY_MB_VALUE} MB\" | systemd-cat -p info
else
echo \"usbcore.usbfs_memory_mb: Could not be modified. Error: ${?}\" | systemd-cat -p error
fi
exit 0" > $DEF_DIRECTORY/Scripts/resize_usbfs_buffersize.sh

        $SUDO chmod +x $DEF_DIRECTORY/Scripts/resize_usbfs_buffersize.sh
        echo -e "
[Unit]
Description=MAXTRIX VISION - USB buffer size modification service

Wants=network.target
After=syslog.target network-online.target

[Service]
Type=simple
ExecStart=$DEF_DIRECTORY/Scripts/resize_usbfs_buffersize.sh
Restart=on-failure
RestartSec=10
KillMode=process

[Install]
WantedBy=multi-user.target
" > /tmp/resize_usbfs_buffersize.service
      $SUDO mv /tmp/resize_usbfs_buffersize.service /etc/systemd/system
      $SUDO systemctl enable resize_usbfs_buffersize.service
      $SUDO systemctl start resize_usbfs_buffersize.service    
    else
        echo "Error: Init system does not support usbfs-modifications!"
    fi
  fi

  # Check whether the network buffers are configured
  if [ "$GEV_SUPPORT" == "TRUE" ]; then
  ERROR=0
  echo "------------------------------------GEV Check--------------------------------------"
      if [ "$(which sysctl)" == "" ]; then
         echo "Warning: 'sysctl' not present on the system, network parameters cannot be checked!"
         ERROR=1
      else
         RMEM=$(( $(sysctl -n net.core.rmem_max) / 1048576 ))
         WMEM=$(( $(sysctl -n net.core.wmem_max) / 1048576 ))
         BKLG=$(sysctl -n net.core.netdev_max_backlog) 
         if [ $RMEM -lt 16 ]; then
             if [ $RMEM -lt 1 ]; then
                 echo "Warning: 'net.core.rmem_max' Receive buffer settings are low( less than 1MB )!"
             else
                 echo "Warning: 'net.core.rmem_max' Receive buffer settings are low($RMEM MB)!"
             fi
             ERROR=1
         fi
         if [ $WMEM -lt 4 ]; then
             if [ $WMEM -lt 1 ]; then
                 echo "Warning: 'net.core.rmem_max' Transmit buffer settings are low( less than 1MB )!"
             else
                 echo "Warning: 'net.core.rmem_max' Transmit buffer settings are low($WMEM MB)!"
             fi
             ERROR=1
         fi
         if [ $BKLG -lt 10000 ]; then
             echo "Warning: 'net.core.netdev_max_backlog' input queue settings are low($BKLG elements)!"
             ERROR=1
         fi
         if [ $ERROR == 1 ]; then
             echo "Not all network parameters are optimized. Incomplete frames may occur during image acquisition!"
         fi
     fi
     if [ $ERROR == 1 ]; then
         let WARNING_NUMBER=WARNING_NUMBER+1
         echo
         echo "Please refer to 'Quickstart/Optimizing the network configuration' section of the "
         echo "User Manual for more information on how to adjust the network buffers"
         echo "http://www.matrix-vision.com/manuals/mvBlueCOUGAR-X/mvBC_page_quickstart.html#mvBC_subsubsection_quickstart_network_configuration_controller"
         echo "-----------------------------------------------------------------------------------"
     else
        echo "${green}${bold}                                       OK!${reset}                                         "
        echo "-----------------------------------------------------------------------------------"
     fi
  fi
  
  # Check whether the USBFS Memory is configured
  if [ "$U3V_SUPPORT" == "TRUE" ]; then
  ERROR=0
  echo "------------------------------------U3V Check--------------------------------------"
      if [ ! -r /sys/module/usbcore/parameters/usbfs_memory_mb ]; then
         echo "Warning: 'usbfs_memory_mb' parameter does not exist or cannot be read!"
         ERROR=1
      else
         USBMEM=$(cat /sys/module/usbcore/parameters/usbfs_memory_mb)
         if [ $USBMEM -lt 256 ] && [ ! $USBFS_CONFIGURED_PER_SYSTEMD="YES" ]; then
             echo "Warning: 'usbfs_memory_mb' Kernel USB file system buffer settings are low($USBMEM MB)!"
             echo "Incomplete frames may occur during image acquisition!"
             ERROR=1
          fi
     fi
     if [ $ERROR == 1 ]; then
         let WARNING_NUMBER=WARNING_NUMBER+1
         echo
         echo "Please refer to 'Quickstart/Linux/Optimizing USB performance' section of the "
         echo "User Manual for more information on how to adjust the kernel USB buffers"
         echo
         echo "https://www.matrix-vision.com/manuals/mvBlueFOX3/index.html"
         echo " -> Troubleshooting"
         echo "  -> USB3.0 Issues"
         echo "   -> How can I improve the USB 3.0 enviroment?"
         echo "    -> Checklist for Linux"
         echo "     -> Kernel memory"
         echo "-----------------------------------------------------------------------------------"
     else
        echo "${green}${bold}                                       OK!${reset}                                         "
        echo "-----------------------------------------------------------------------------------"
     fi
  fi
fi

# remove all example application sources in case of minimal installation 
if [ "$MINIMAL_INSTALLATION" == "YES" ] ; then
  $SUDO rm -rf $DEF_DIRECTORY/apps >/dev/null 2>&1
fi

echo
source $GENICAM_EXPORT_FILE
echo
echo
if [ "$ERROR_NUMBER" == 0 ] && [ "$WARNING_NUMBER" == 0 ]; then
    echo "-----------------------------------------------------------------------------------"
    echo "${green}${bold}                           Installation successful!${reset}         "
    echo "-----------------------------------------------------------------------------------"
elif [ "$ERROR_NUMBER" == 0 ] && [ "$WARNING_NUMBER" != 0 ]; then
    echo "-----------------------------------------------------------------------------------"
    echo "${yellow}${bold}                           Installation successful!${reset}        "
    echo "-----------------------------------------------------------------------------------"
    echo "                                                                                   "
    echo "  Some warnings have been issued during the installation. Typically the driver     "
    echo "  will work, but some functionalities are missing e.g. some sample applications    "
    echo "  which could not be built because of missing dependencies or not optimized NIC-   "
    echo "  settings.                                                                        "
    echo "                                                                                   "
    echo "  Please refer to the output of the script for further details.                    "
    echo "-----------------------------------------------------------------------------------"
else
    echo "-----------------------------------------------------------------------------------"
    echo "${red}${bold}                        Installation NOT successful!${reset}          "
    echo "-----------------------------------------------------------------------------------"
    echo "                                                                                   "
    echo "  Please provide the full output of this installation script to the MATRIX VISION  "
    echo "  support department if the error messages shown during the installation procedure "
    echo "  don't help you to get the driver package installed correctly!                    "
    echo "-----------------------------------------------------------------------------------"
fi
echo
echo "Do you want to reboot now (default is 'yes')?"
echo "Hit 'n' + <Enter> for 'no', or just <Enter> for 'yes'."
if [ "$USE_DEFAULTS" == "NO" ] ; then
  read YES_NO
else
  YES_NO="n"
fi
if [ "$YES_NO" == "n" ] || [ "$YES_NO" == "N" ]; then
   echo "You need to reboot manually to complete the installation."
else
   $SUDO shutdown -r now
fi
