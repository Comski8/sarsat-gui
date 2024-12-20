#! /bin/bash
sudo apt-get update
sudo apt-get install gcc build-essential
gcc ./dec406_V7.c -lm -o ./dec406_V7
gcc ./reset_usb.c -lm -o ./reset_usb
sudo apt-get install lxterminal
sudo apt-get install sox
sudo apt-get install libsox-fmt-all
sudo apt-get install pulseaudio
sudo apt-get install rtl-sdr
sudo apt-get install perl
sudo apt-get install sendemail
sudo chmod a+x scan406.pl
sudo chmod a+x sc*
sudo chmod a+x dec*
sudo chmod a+x reset_usb
sudo chmod a+x config_mail.pl
./config_mail.pl
echo -e "\nVoulez redémarrer maintenant? (o/n)"
read reponse

if [[ "$reponse" ==  *o* ]]
then
	sudo reboot
fi
 	
