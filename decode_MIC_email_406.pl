#!/usr/bin/env perl
# Script perl pour décoder les balises 406MHz par l'entrée MICRO avec envoi automatique de MAIL
# sox dirige le flux audio de l'entrée micro vers mon logiciel de décodage "dec406_v7"
# Brancher l'entrée MICRO puis pour lancer le décodage entrer la commande:
# ./MIC_email_406.pl 
#                                                                           F4EHY 18-03-2021

use POSIX qw(strftime);
use strict;
use warnings;

my $line;
my $utc;
my $date=0;
my $time=0;

my @db;
my $i;
my $j;
my $frq=0;
my $dec = './dec406_V7 --100 --M3 --une_minute';
my $dec1 = './dec406_V7 --100 --M3 --une_minute --osm'; 
my $filter = "lowpass 3000 highpass 400"; #highpass de 10Hz à 400Hz selon la qualité du signal

#my $largeur = "12k";
#my $WFM="";
#my $squelch=-200;
#my $snr = 6;
my $trouve;
#my $mean;
my $smtp_serveur='smtp.gmail.com:587';
my $utilisateur='toto@gmail.com';
my $password='mot_de_passe_mail';
my $destinataires='dede@free.fr,lili@orange.fr';
lit_config_mail();
print "$smtp_serveur\n$utilisateur\n$password \n$destinataires\n";

my $var=@ARGV;
print "\n SYNTAXE:  MIC_406.pl  [osm]\n";
print " Exemple:\n	MIC_406.pl \n	MIC_406.pl osm\n\n";
	
for (my $i=0;$i<@ARGV;$i++)
{
	#print "\n $i--> $ARGV[$i]";
	if ($i==0)
	{  if ($ARGV[0] eq 'osm'){$dec=$dec1;}
	}
		
}


while (1) {
    
    do{
    print "\n\nLancement du Decodage   ";
    $utc = strftime(' %d %m %Y   %Hh%Mm%Ss', gmtime);
    print " UTC $utc";
    system("timeout 56s  sox -d -t wav - $filter 2>/dev/null |\ 
	    $dec 1>./trame 2>./code ");  
      
    $trouve="PAS encore trouve";
    my $ligne;
    if (open (F2, '<', './code')) {
	while (defined ($ligne = <F2>)) {
	    chomp $ligne;
	    #print "\nligne : $ligne\n";
	    if ($ligne eq 'TROUVE') {$trouve='TROUVE';}
	    }
	close F2;
	}

    affiche_trame();
    if($trouve eq 'TROUVE'){envoi_mail();}

    } while ($trouve eq 'TROUVE');

}


sub envoi_mail {
    # Lire la trame complète depuis le fichier ./trame
    my $trame_complete = "Trame non disponible";

    if (open(my $fh, '<', './trame')) {
        $trame_complete = join('', <$fh>); # Lire tout le contenu de ./trame
        close($fh);
    }

    # Construire le message avec la trame complète
    my $m='"Trame détectée : '.$trame_complete.'"';
    my $a='"./trame"'; # Toujours attacher le fichier ./trame
    my $u='"Alerte_Balise_406"';
    my $l='email.log';
    my $s='"'.$smtp_serveur.'"';
    my $o='tls=yes';
    my $f='"'.$utilisateur.'"';
    my $xu='"'.$utilisateur.'"';
    my $xp='"'.$password.'"';
    my $t='"'.$destinataires.'"';

    # Commande pour envoyer l'e-mail
    system("sendemail -l $l -f $f -u $u -t $t -s $s -o $o -xu $xu -xp $xp -m $m -a $a 2>/dev/null 1>/dev/null");
}


# affichage fichier ./trame
sub affiche_trame {
					my $ligne;
					if (open (F3, '<', './trame')) {
						while (defined ($ligne = <F3>)) {
							#chomp $ligne;
							print "$ligne";
							}
						close F3;
					}
}

#lire config_mail.txt
sub lit_config_mail {
		my ($k, $v);
		my %h;
		if (open(F4, "<config_mail.txt")){		
			#copie key/value depuis le fichier 'config_mail' dans hash.
			while (<F4>) {
				chomp;
				($k, $v) = split(/=/);
				$h{$k} = $v;
			}
			close F4; 
			$utilisateur=$h{'utilisateur'};
			$password=$h{'password'};
			$smtp_serveur=$h{'smtp_serveur'};
			$destinataires=$h{'destinataires'};
		}
}
