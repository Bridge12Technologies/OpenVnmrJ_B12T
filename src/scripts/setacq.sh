#!/bin/bash
#
# Copyright (C) 2015  University of Oregon
# 
# You may distribute under the terms of either the GNU General Public
# License or the Apache License, as specified in the LICENSE file.
# 
# For more information, see the LICENSE file.
# 
#

# set -x

SCRIPT=$(basename "$0")
vnmrsystem="/vnmr"

#  MAIN Main main
#----------------------------
if [[ ! x$(uname -s) = "xLinux" ]] ; then
   echo " "
   echo "$SCRIPT suitable for Linux-based systems only"
   echo " "
   exit 0
fi

#-----------------------------------------------------------------
# make sure no OpenVnmrJ is running
#
npids=$(ps -e  | grep Vnmr | awk '{ printf("%d ",$1) }')
if [[ x"$npids" != "x" ]] ; then 
   echo ""
   echo "You must exit all 'OpenVnmrJ'-s before running $SCRIPT"
   echo "Please exit from OpenVnmrJ then restart $SCRIPT"
   echo ""
   exit 0
fi

#-----------------------------------------------------------------
#  Need to be root to configure an acquisition system

userId=$(/usr/bin/id | awk 'BEGIN { FS = " " } { print $1 }')
if [[ $userId != "uid=0(root)" ]]; then
   echo
   echo "To run $0 you will need to be the system's root user,"
   echo "or type cntrl-C to exit."
   echo
   s=1
   t=3
   while [[ $s = 1 ]] && [[ ! $t = 0 ]]; do
      echo "Please enter this system's root user password"
      echo
      if [ -f /etc/debian_version ]; then
         sudo $0 $* ;
      else
         su root -c "$0 $*";
      fi
      s=$?
      t=$((t-1))
      echo " "
   done
   if [[ $t = 0 ]]; then
      echo "Access denied. Type cntrl-C to exit this window."
      echo
   fi
exit 0
fi

#-----------------------------------------------------------------
# initialize some variables
#
reboot=0
postfix=$(date +"%F_%T")
logfile=${vnmrsystem}/adm/log/setacq_$postfix
echo "Log file is $logfile"

#-----------------------------------------------------------------
# Check if the Expproc is still running. If so stop it
#-----------------------------------------------------------------
npids=$(pgrep Expproc)
if [[ ! -z $npids ]] ; then 
   ${vnmrsystem}/acqbin/startStopProcs
fi

if [ ! -f /etc/udev/rules.d/99-spincore.rules ]
then
   echo "Installing SpinCore USB rules"
   echo "Installing SpinCore USB rules" &>> $logfile
   cp -f $vnmrsystem/acqbin/99-spincore.rules /etc/udev/rules.d/.
   udevadm control --reload-rules
fi

systemctl is-active --quiet rpcbind.service
if [[ $? -ne 0 ]]; then
   systemctl add-wants multi-user rpcbind.service &>> $logfile
   reboot=1
fi

#-----------------------------------------------------------------
# Arrange for procs to start at system bootup
#-----------------------------------------------------------------
rm -f /etc/init.d/rc.vnmr

sysdDir=$(pkg-config systemd --variable=systemdsystemunitdir)
rm -f $sysdDir/vnmr.service
cp $vnmrsystem/acqbin/vnmr.service $sysdDir/.
chmod 644 $sysdDir/vnmr.service
systemctl enable --quiet vnmr.service
systemctl daemon-reload

touch $vnmrsystem/acqbin/acqpresent
owner=$(ls -l $vnmrsystem/vnmrrev | awk '{ printf($3) }')
group=$(ls -l $vnmrsystem/vnmrrev | awk '{ printf($4) }')
chown $owner:$group $vnmrsystem/acqbin/acqpresent
chmod 644 $vnmrsystem/acqbin/acqpresent

#-----------------------------------------------------------------
# Remove some files (Queues) NOT IPC_V_SEM_DBM
# Because cleanliness is next to ... you know.
#-----------------------------------------------------------------
rm -f /tmp/ExpQs
rm -f /tmp/ExpActiveQ
rm -f /tmp/msgQKeyDbm
rm -f /tmp/ProcQs
rm -f /tmp/ActiveQ
rm -f /vnmr/acqqueue/ExpStatus

export vnmrsystem
#  ${vnmrsystem}/bin/makesuacqproc silent

#-----------------------------------------------------------------
if [ $reboot -eq 1 ]
then
   echo " "
   echo "You must reboot Linux for these changes to take effect"
   echo "Enter 'sudo reboot' to reboot Linux"
else
   ${vnmrsystem}/acqbin/startStopProcs
fi
echo ""
${vnmrsystem}/bin/acqcomm help
echo ""

