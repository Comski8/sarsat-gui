/****************************************************************************************/
/* Décodage de Balises de détresse 406Mhz ou d'exercice 432                             */
/* à partir d'un flux audio provenant:                                                  */
/*   - d'un logiciel sdr (gqrx par exemple)                                             */
/*   - ou de l'entrée Micro reliée à la sortie "discri" d'un transceiver                */
/*   - ou de la lecture d'un enregistrement audio  ".wav"                               */
/*                                                                                      */
/*                                           F4EHY 10-7-2020 version  V6.0              */
/*                                                                                      */
/****************************************************************************************/ 
/*--------------------------------------------------------------------------------------*/
/* Compilation du fichier source:                                                       */
/*      gcc dec406.c -lm -o dec406                                                      */     
/*--------------------------------------------------------------------------------------*/
/* Options:                                                                             */
/*      --osm  pour affichage sur une carte OpenStreetMap                               */
/*      --help aide                                                                     */
/*---------------------------------------------------------------------------------------------*/
/* Lancement du programme:                                                                     */
/* sox -t alsa default -t wav - lowpass 3000 highpass 10 gain -l 6 2>/dev/null |./dec406       */
/* sox -t alsa default -t wav - lowpass 3000 highpass 10 gain -l 6 2>/dev/null |./dec406 --osm */ 
/*---------------------------------------------------------------------------------------------*/
/* Installer 'sox' s'il est absent:                                                     */
/*      sudo apt-get install sox                                                        */    
/* 'sox' met en forme le flux audio qui est redirigé vers 'dec406' pour le décodage     */
/*--------------------------------------------------------------------------------------*/

/*Exemple de trame longue avec GPS (144 bits) 
1111111111111110110100001000111000111111001100111110101111001011111011110000001100100100001010011011111101110111000100100000010000001101011010001
Contenu utile en Hexa bits 25 à 144:
8e3f33ebcbef032429bf7712040d68
*/
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
typedef unsigned char  uin8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
#define  PI 4*atan(1)
#define SEUIL 2000 //3000
int bauds=400;      //400 bits par secondes
int f_ech = 0;      //frequence echantillonnage
int ech_par_bit = 0;//f_ech/baud
int longueur_trame=144;   //144  ou 112 bits avec la synchro
int lon=30;         //30 ou 22 longueur message hexa en quartets(4bits)  (debut message :bit 25)
int bits = 0;       //codage des echantillons 8 ou 16 bits 
int N_canaux = 0;   //Nbre de canaux audio
int opt_osm = 0;    //argument pour affichage sur OpenStreetMap
int opt_minute = 0;
int flux_wav = 1;   //flux audio via stdin ou fichier .wav   
int canal_audio = 0;//canal audio
int n_ech = 0;      //numero de l'echantillon
int seuilM=SEUIL;   //seuil pics ou fronts positifs 
int seuilm=-SEUIL;  //seuil pics ou fronts negatifs
int seuil=-SEUIL;   //seuil actif

char s[200];        // 144 bits maxi+ 0 terminal

int calcul(int a, int b)
    {int i,y=0,x=1;
    for (i=b;i>=a;i--)
        {if(s[i]=='1') y=y+x;
        x=2*x;
        }
    return y;
    }
void envoi_byte(int x){
    int a,b;
    //String aa,bb;
    a=x/16;
    b=x%16;
    printf("%x",a);
    printf("%x",b);
    //aa= //Integer.toHex//String(a);
    //bb= //Integer.toHex//String(b);
    //contenu.append(aa);
    //contenu.append(bb);
}
int afficher_carte_osm(double llatd, double llngd){
    char ch[200];
    float lat,lng;
    int a;
    lat=(float)llatd;lng=(float)llngd;
    //Navigateur chromium-browser ou firefox
    FILE * file;
   file = fopen("/usr/bin/chromium-browser", "r");
   
    if (file){//chromium-browser existe
        sprintf(ch,"/usr/bin/chromium-browser --disable-gpu 'https://www.openstreetmap.org/?mlat=%f&mlon=%f#map=6/%f/%f &'> /dev/null 2>&1",lat,lng,lat,lng);
        fclose(file);
        a=system(ch);
        }
   else {
        file=fopen("/usr/bin/firefox", "r");
        if (file){ 
            fclose(file);
            sprintf(ch,"/usr/bin/firefox 'https://www.openstreetmap.org/?mlat=%f&mlon=%f#map=6/%f/%f & '> /dev/null 2>&1",lat,lng,lat,lng);
            system(ch);  
            //execlp("/usr/bin/firefox","/usr/bin/firefox ", ch,  (char *) 0);
        }
        else printf("\n*Installer Chromium-browser ou Firefox pour afficher la position avec OpenStreeMap...");
       }
return 0;
}

void  GeogToUTM(double llatd, double llngd){
		//Merci à Daniel-Bryant
        //https://github.com/daniel-bryant/CoordinateConversion/blob/master/conversion.cpp
          char ch[200];
          float lat,lng;
          lat=(float)llatd;lng=(float)llngd;
		    if ((llatd>127)||(llngd>255)||(llatd<0)||(llngd<0))
                {printf("\n\rCoordonnées invalides!! ");
                }
            else{
                if (opt_osm)  afficher_carte_osm(llatd,llngd); 
                sprintf(ch,"https://www.openstreetmap.org/?mlat=%f&mlon=%f#map=6/%f/%f ",lat,lng,lat,lng);
                double DatumEqRad[] = {6378137.0,6378137.0,6378137.0,6378135.0,6378160.0,6378245.0,6378206.4,
                6378388.0,6378388.0,6378249.1,6378206.4,6377563.4,6377397.2,6377276.3};	
                double DatumFlat[] = {298.2572236, 298.2572236, 298.2572215,	298.2597208, 298.2497323, 298.2997381, 294.9786982,
                296.9993621, 296.9993621, 293.4660167, 294.9786982, 299.3247788, 299.1527052, 300.8021499}; 
                int Item = 0;//Default WGS84
                double k0 = 0.9996;//scale on central meridian
                double a = DatumEqRad[Item];//equatorial radius, meters du WGS84. 
                double f = 1/DatumFlat[Item];//polar flattening du WGS84.
                double b = a*(1-f);//polar axis.
                double e = sqrt(1 - b*b/a*a);//eccentricity
                double drad = PI/180;//Convert degrees to radians)
                double latd = 0;//latitude in degrees
                double phi = 0;//latitude (north +, south -), but uses phi in reference
                double e0 = e/sqrt(1 - e*e);//e prime in reference
                double N = a/sqrt(1-pow((e*sin(phi)),2));
                double T = pow(tan(phi),2);
                double C = pow(e*cos(phi),2);
                double lng = 0;//Longitude (e = +, w = -) - can't use long - reserved word
                double lng0 = 0;//longitude of central meridian
                double lngd = 0;//longitude in degrees
                double M = 0;//M requires calculation
                double x = 0;//x coordinate
                double y = 0;//y coordinate
                double k = 1;//local scale
                double utmz = 30;//utm zone milieu
                double zcm = 0;//zone meridien centrale
                char DigraphLetrsE[] = {'A','B', 'C','D', 'E', 'F', 'G', 'H','J','K','L','M','N','P','Q','R','S','T','U','V','W','X','Y','Z'};
                char DigraphLetrsN[] = {'A','B', 'C','D', 'E', 'F', 'G', 'H','J','K','L','M','N','P','Q','R','S','T','U','V'};
                k0 = 0.9996;//scale on central meridian
                b = a*(1-f);//polar axis.
                e = sqrt(1 - (b/a)*(b/a));//eccentricity
                // --- conversion en UTM --------------------------------
                latd=llatd;
                lngd=llngd;
                phi = latd*drad;//Convert latitude to radians
                lng = lngd*drad;//Convert longitude to radians
                utmz = 1 + floor((lngd+180)/6);//calculate utm zone
                double latz = 0;//Latitude zone: A-B S of -80, C-W -80 to +72, X 72-84, Y,Z N of 84
                if (latd > -80 && latd < 72){latz = floor((latd + 80)/8)+2;}
                if (latd > 72 && latd < 84){latz = 21;}
                if (latd > 84){latz = 23;}
                zcm = 3 + 6*(utmz-1) - 180;//Central meridian of zone
                //Calcul Intermediate Terms
                e0 = e/sqrt(1 - e*e);//Called e prime in reference
                double esq = (1 - (b/a)*(b/a));//e squared for use in expansions
                double e0sq = e*e/(1-e*e);// e0 squared - always even powers
                N = a/sqrt(1-pow((e*sin(phi)),2));
                T = pow(tan(phi),2);
                C = e0sq*pow(cos(phi),2);
                double A = (lngd-zcm)*drad*cos(phi);               
                //Calcul M
                M = phi*(1 - esq*(1/4 + esq*(3/64 + 5*esq/256)));
                M = M - sin(2*phi)*(esq*(3/8 + esq*(3/32 + 45*esq/1024)));
                M = M + sin(4*phi)*(esq*esq*(15/256 + esq*45/1024));
                M = M - sin(6*phi)*(esq*esq*esq*(35/3072));
                M = M*a;//Arc length along standard meridian
                double EE=0.0818192;
                double SS=(1-e*e/4-3*e*e*e*e/64-5*e*e*e*e*e*e/256)*phi;
                        SS=SS-(3*e*e/8+3*e*e*e*e/32+45*e*e*e*e*e*e/1024)*sin(2*phi);
                        SS=SS+(15*e*e*e*e/256+45*e*e*e*e*e*e/1024)*sin(4*phi);
                        SS=SS-(35*e*e*e*e*e*e/3072)*sin(6*phi);
                double NU=1/sqrt(1-EE*EE*sin(phi)*sin(phi));	
                x=500000+0.9996*a*NU*(A+(1-T+C)*A*A*A/6+(5-18*T+T*T)*A*A*A*A*A/120) ;
                y= 0.9996*a*(SS+NU*tan(phi)*(A*A/2+(5-T+9*C+4*C*C)*A*A*A*A/24+(61-58*T+T*T)*A*A*A*A*A*A/720));
                if (y < 0){y = 10000000+y;}
                x= round(10*(x))/10;
                y= round(10*y)/10;  
                //Genere les lettres des Zones
                char C0 = DigraphLetrsE[(int)latz];
                double Letr = floor((utmz-1)*8 + (x)/100000);
                Letr = Letr - 24*floor(Letr/24)-1;
                char C1 = DigraphLetrsE[(int)Letr];
                Letr = floor(y/100000);
                if (utmz/2 == floor(utmz/2)){Letr = Letr+5;}
                Letr = Letr - 20*floor(Letr/20);
                char C2 =  DigraphLetrsN[(int)Letr];
                printf("\n\rUTM Zone: %1.0f%c",utmz,C0);
                printf("%c%c",C1,C2);
                printf(" x: %1.0fm  y: %1.0fm ",x,y);
                printf("\n\rCliquez sur le lien pour afficher la carte dans le navigateur;");
                
                printf("\n\r%s",ch);
                //contenu.append("UTM: Zone:").append(decform.format(utmz)).append(//Charact.to//String(C0));
                //contenu.append(" ").append(//Charact.to//String(C1)).append(//Charact.to//String(C2));
                //contenu.append(" x: ").append(decform.format(x)).append("m");
                //contenu.append(" y: ").append(decform.format(y)).append("m\r\n");
             }
}

void standard_test()
    {   int i,j,a;
        for (i=40;i<64;i++)
            {//Charact ch=s[i];
            printf("%c",s[i]);
            //contenu.append(ch.toString());
            }
        printf(" en Hexa:");
        //contenu.append(" en Hexa: ");
        for(j=0;j<3;j++)
                                {i=40+j*8;                              
                                a=calcul(i,i+7);
                                envoi_byte(a);
                                } 
    }

char  baudot(int x)
	{char y;
	switch(x)
		{case 56: y='A';break;
		case 51: y='B';break;
		case 46: y='C';break;
		case 50: y='D';break;
		case 48: y='E';break;
		case 54: y='F';break;
		case 43: y='G';break;
		case 37: y='H';break;
		case 44: y='I';break;
		case 58: y='J';break;
		case 62: y='K';break;
		case 41: y='L';break;
		case 39: y='M';break;
		case 38: y='N';break;
		case 35: y='O';break;
		case 45: y='P';break;
		case 61: y='Q';break;
		case 42: y='R';break;
		case 52: y='S';break;
		case 33: y='T';break;
		case 60: y='U';break;
		case 47: y='V';break;
		case 57: y='W';break;
		case 55: y='X';break;
		case 53: y='Y';break;
		case 49: y='Z';break;
		case 36: y=' ';break;
		case 24: y='-';break;
		case 23: y='/';break;
		case 13: y='0';break;
		case 29: y='1';break;
		case 25: y='2';break;
		case 16: y='3';break;
		case 10: y='4';break;
		case 1: y='5';break;
		case 21: y='6';break;
		case 28: y='7';break;
		case 12: y='8';break;
		case 3: y='9';break;
		default: y='_';
		}
	return y;
	}  

  void affiche_baudot42()
	{int i,j;
	 int a;
	for(j=0;j<6;j++)
		{i=39+j*6;
		//a=32*(s[i]=='1')+16*(s[i+1]=='1')+8*(s[i+2]=='1')+4*(s[i+3]=='1')+2*(s[i+4]=='1')+(s[i+5]=='1');
		a=calcul(i,i+5);
        printf("%c",baudot(a));
        //Charact ch = baudot(a);
		//contenu.append(ch.to//String());
		}
         //printf("\r\n"); //contenu.append("\r\n") ;
	   }

void affiche_baudot_2()
	{int i,j;
	 int a;
	for(j=0;j<7;j++)
		{i=39+j*6;
		//a=32*(s[i]=='1')+16*(s[i+1]=='1')+8*(s[i+2]=='1')+4*(s[i+3]=='1')+2*(s[i+4]=='1')+(s[i+5]=='1');
		a=calcul(i,i+5);
        printf("%c",baudot(a));
        //Charact ch = baudot(a);
		//contenu.append(ch.to//String());
		}
         //printf("\r\n"); 
         //contenu.append("\r\n") ; 
	   }
void specific_beacon()
	{int i,j;
	int a;
	printf("\n\rSpecific beacon: ");//contenu.append("Specific beacon: ");
	for(j=0;j<1;j++)
		{i=75+j*6;
		//a=32*(s[i]=='1')+16*(s[i+1]=='1')+8*(s[i+2]=='1')+4*(s[i+3]=='1')+2*(s[i+4]=='1')+(s[i+5]=='1');
		a=calcul(i,i+5);
        printf("%c",baudot(a));
		//Charact ch = baudot(a);
		//contenu.append(ch.to//String());
		}
         //   printf("\r\n");   //contenu.append("\r\n"); 
	   }


void affiche_baudot_1()
	{
      int i;
      int j;
      int b;
      int c;
      int d;
	  int a;
      printf("\n\rRadio Call Sign Identification:");
	 //contenu.append("Radio Call Sign Identification: ") ; 	
	  for(j=0;j<4;j++)
		{
        i=39+j*6;
		//a=32*(s[i]=='1')+16*(s[i+1]=='1')+8*(s[i+2]=='1')+4*(s[i+3]=='1')+2*(s[i+4]=='1')+(s[i+5]=='1');
		a=calcul(i,i+5);
                printf("%c",baudot(a));
                //Charact ch = baudot(a);
              	//contenu.append(ch.to//String());
		}
         ////contenu.append("\r\n"); 
         printf(" ");
        i=63;
        //b=8*(s[i]=='1')+4*(s[i+1]=='1')+2*(s[i+2]=='1')+(s[i+3]=='1');
        b=calcul(i,i+3);
        //c=8*(s[i+4]=='1')+4*(s[i+5]=='1')+2*(s[i+6]=='1')+(s[i+7]=='1');
        c=calcul(i+4,i+7);
        //d=8*(s[i+8]=='1')+4*(s[i+9]=='1')+2*(s[i+10]=='1')+(s[i+11]=='1'); 
        d=calcul(i+8,i+11);
        printf("%d",b);
        printf("%d",c);
        printf("%d",d);
        //contenu.append(b);
        //contenu.append(c);
        //contenu.append(d);
        //contenu.append("\r\n"); 
	   }
void affiche_baudot()
	{
    int i,j,a;
	for(j=0;j<4;j++)
		{i=39+j*6;
		//a=32*(s[i]=='1')+16*(s[i+1]=='1')+8*(s[i+2]=='1')+4*(s[i+3]=='1')+2*(s[i+4]=='1')+(s[i+5]=='1');
		a=calcul(i,i+5);
        printf("%c",baudot(a));
        //Charact ch = baudot(a);
		//contenu.append(ch.to//String());
		}
        i=63;
        //a=2048*(s[i]=='1')+1024*(s[i+1]=='1')+512*(s[i+2]=='1')+256*(s[i+3]=='1')+128*(s[i+4]=='1')+64*(s[i+5]=='1')+32*(s[i+6]=='1')+16*(s[i+7]=='1')+8*(s[i+8]=='1')+4*(s[i+9]=='1')+2*(s[i+10]=='1')+(s[i+11]=='1');
        a=calcul(i,i+11);
        printf("%d",a);
        //Intege aa= a;
        //contenu.append(aa.to//String());
        i=81;
        //a=2*(s[i]=='0')+(s[i+1]=='0');
        a=calcul(i,i+1);
        printf("%d",a);
        //aa= a;
        //contenu.append(aa.to//String());
    }

void affiche_baudot30()
	{int i,j,a;
	for(j=0;j<7;j++)
		{i=43+j*6;
		//a=32*(s[i]=='1')+16*(s[i+1]=='1')+8*(s[i+2]=='1')+4*(s[i+3]=='1')+2*(s[i+4]=='1')+(s[i+5]=='1');
		a=calcul(i,i+5);
        printf("%c",baudot(a));
        //Charact ch = baudot(a);
		//contenu.append(ch.to//String());
		}
	}

void localisation_standard()
	{char  c ;
    int i,x,latD,latM,latS,lonD,lonM,lonS;
	if (s[64]=='0') c='N'; else c='S';
    //DecimalFormat decform = new DecimalFormat("###.########");
	i=65;
	//latD=64*(s[i]=='1')+32*(s[i+1]=='1')+16*(s[i+2]=='1')+8*(s[i+3]=='1')+4*(s[i+4]=='1')+2*(s[i+5]=='1')+(s[i+6]=='1');
    latD=calcul(i,i+6);
    //latM=15*(2*(s[i+7]=='1')+(s[i+8]=='1'));
    latM=15*calcul(i+7,i+8);
	//ofsset minutes et secondes
	i=113;
	//x=16*(s[i]=='1')+8*(s[i+1]=='1')+4*(s[i+2]=='1')+2*(s[i+3]=='1')+(s[i+4]=='1');
	x=calcul(i,i+4);
    if(s[112]=='0') {x=-x;}
	latM+=x; 
	i=118;
	//latS=4*(8*(s[i]=='1')+4*(s[i+1]=='1')+2*(s[i+2]=='1')+(s[i+3]=='1'));
	latS=4*calcul(i,i+3);
    if(s[112]=='0')   //latS=-latS;
				{latS=60-latS;latM--;
					if (latM<0) {latM=60+latM;latD--;}	
				}
	//printf("Latitude: ");printf("%c",c);printf(" ");printf("%d",latD);printf("d");printf("%d",latM);printf("m");printf("%d",latS);printf("s"); 
	printf("\n\rLatitude : %c %dd%dm%ds ",c,latD,latM,latS);
    //Charact cc = c;//Integer llatD = latD;//Integer llatM = latM;//Integer llatS = latS;
    ////contenu.append("Latitude  : "+cc.to//String()+" "+llatD.to//String()+"d"+llatM.to//String()+"m"+llatS.to//String()+"s\r\n");
    //contenu.append("\r\nLatitude : ").append(cc.to//String()).append(" ").append(llatD.to//String()).append("d").append(llatM.to//String()).append("m").append(llatS.to//String()).append("s");
    double gpsLat= latD+(60.0*latM+latS)/3600.0;
    printf("\t %2.6f Deg",gpsLat);
    //contenu.append(" = ").append(decform.format(gpsLat)).append(" Deg\r\n");
	if (s[74]=='0') {c='E';} else {c='W';}
	i=75;
	//lonD=128*(s[i]=='1')+64*(s[i+1]=='1')+32*(s[i+2]=='1')+16*(s[i+3]=='1')+8*(s[i+4]=='1')+4*(s[i+5]=='1')+2*(s[i+6]=='1')+(s[i+7]=='1');
    lonD=calcul(i,i+7);
    //lonM=15*(2*(s[i+8]=='1')+(s[i+9]=='1'));
    lonM=15*calcul(i+8,i+9);
	//ofsset minutes et secondes
	i=123;
	//x=16*(s[i]=='1')+8*(s[i+1]=='1')+4*(s[i+2]=='1')+2*(s[i+3]=='1')+(s[i+4]=='1');
	x=calcul(i,i+4);
    if(s[122]=='0') {x=-x;}
	lonM+=x; 
	i=128;
	//lonS=4*(8*(s[i]=='1')+4*(s[i+1]=='1')+2*(s[i+2]=='1')+(s[i+3]=='1'));
	lonS=4*calcul(i,i+3);
    if(s[122]=='0') //{lonS=-lonS;} 
				{lonS=60-lonS;lonM--;
					if (lonM<0) {lonM=60+lonM;lonD--;}	
				}
	
	//printf("Longitude: %c %dd%dm%ds ");printf(c);printf(' ');printf(lonD);printf('d');printf(lonM);printf('m');printf(lonS);printf('s'); 
    printf("\n\rLongitude: %c %dd%dm%ds ",c,lonD,lonM,lonS); 
    //Charact ccc = c;//Integer llonD = lonD;//Integer llonM = lonM;//Integer llonS = lonS;
    //contenu.append("Longitude: "+ccc.to//String()+" "+llonD.to//String()+"d"+llonM.to//String()+"m"+llonS.to//String()+"s\r\n");
    //contenu.append("Longitude: ").append(ccc.to//String()).append(" ").append(llonD.to//String()).append("d").append(llonM.to//String()).append("m").append(llonS.to//String()).append("s");
    double gpsLon= lonD+(60.0*lonM+lonS)/3600.0;
    printf("\t %3.6f Deg\n",gpsLon);
    //contenu.append(" = ").append(decform.format(gpsLon)).append(" Deg\r\n");
    GeogToUTM(gpsLat, gpsLon);
    }
 
 void supplementary_data()
 {  //s[109]=1;  ?????????????????????????????????????????????
    /*s[108]=0; 
    s[107]=1; 
    s[106]=1;*/
    printf("\n\rFixed bits (1101) Pass");
    //contenu.append("Fixed bits (1101) Pass\r\n");
    if (s[110]=='1') {printf("\n\rEncoded position data source internal");}
                       //contenu.append("Encoded position data source internal\r\n");}
    if (s[111]=='1') {printf("\n\r121.5 MHz Homing");}
                        //contenu.append("121.5 MHz Homing\r\n");} 	   
  
}
  
 void supplementary_data_1()
 {/* s[108]=0;
    s[107]=1; 
    s[106]=1;*/
    if (s[109]=='1') {printf("\n\rAdditional Data Flag:Position");}
                        //contenu.append(" Additional Data Flag:Position\r\n"); }
    if (s[110]=='1') {printf("\n\rEncoded position data source internal");}
                         //contenu.append("Encoded position data source internal\r\n");}
    if (s[111]=='1') {printf("\n\r121.5 MHz Homing");}
                          //contenu.append("121.5 MHz Homing\r\n");}
 }
 
void localisation_standard1()
	{char c; 
    int i,x,latD,latM,latS,lonD,lonM,lonS;
	if (s[64]=='0') c='N'; else c='S';
	i=65;
	//latD=64*(s[i]=='1')+32*(s[i+1]=='1')+16*(s[i+2]=='1')+8*(s[i+3]=='1')+4*(s[i+4]=='1')+2*(s[i+5]=='1')+(s[i+6]=='1');
    latD=calcul(i,i+6);
    //latM=15*(2*(s[i+7]=='1')+(s[i+8]=='1'));
	latM=15*calcul(i+7,i+8);
    //ofsset minutes et secondes
	i=113;
	//x=16*(s[i]=='1')+8*(s[i+1]=='1')+4*(s[i+2]=='1')+2*(s[i+3]=='1')+(s[i+4]=='1');
	x=calcul(i,i+4);
    if(s[112]=='0') {x=-x;}
    latM+=x;
	i=118;
	//latS=4*(8*(s[i]=='1')+4*(s[i+1]=='1')+2*(s[i+2]=='1')+(s[i+3]=='1'));
	latS=4*calcul(i,i+3);
    if(s[112]=='0') //{latS=-latS;} 
   		{latS=60-latS;latM--;
	if (latM<0) {latM=60+latM;latD--;}	
		}
	//printf("Latitude: ");printf(c);printf(' ');printf(latD);printf('d');printf(latM);printf('m');printf(latS);printf('s'); 
    printf("\n\rLatitude : %c %dd%dm%ds ",c,latD,latM,latS);//Charact cc = c;//Integer llatD = latD;//Integer llatM = latM;//Integer llatS = latS;
    //contenu.append("Latitude  : "+cc.to//String()+" "+llatD.to//String()+"d"+llatM.to//String()+"m"+llatS.to//String()+"s  \r\n");
    //contenu.append("\r\nLatitude : ").append(cc.to//String()).append(" ").append(llatD.to//String()).append("d").append(llatM.to//String()).append("m").append(llatS.to//String()).append("s");
    double gpsLat= latD+(60.0*latM+latS)/3600.0;
    printf(" = %2.6f Deg",gpsLat);
    //contenu.append(" = ").append(decform.format(gpsLat)).append(" Deg\r\n");
    if (s[74]=='0') {c='E';} else {c='W';}
	i=75;
	//lonD=128*(s[i]=='1')+64*(s[i+1]=='1')+32*(s[i+2]=='1')+16*(s[i+3]=='1')+8*(s[i+4]=='1')+4*(s[i+5]=='1')+2*(s[i+6]=='1')+(s[i+7]=='1');
    lonD=calcul(i,i+7);
    //lonM=15*(2*(s[i+8]=='1')+(s[i+9]=='1'));
	lonM=15*calcul(i+8,i+9);
    //ofsset minutes et secondes
	i=123;
	//x=16*(s[i]=='1')+8*(s[i+1]=='1')+4*(s[i+2]=='1')+2*(s[i+3]=='1')+(s[i+4]=='1');
	x=calcul(i,i+4);
    if(s[122]=='0') {x=-x;}
    lonM+=x; 
	i=128;
	//lonS=4*(8*(s[i]=='1')+4*(s[i+1]=='1')+2*(s[i+2]=='1')+(s[i+3]=='1'));
	lonS=4*calcul(i,i+3);
    if(s[122]=='0') //{lonS=-lonS;}
		{lonS=60-lonS;lonM--;
        if (lonM<0) {lonM=60+lonM;lonD--;}	
        }   
	 //printf("Longitude: ");printf(c);printf(' ');printf(lonD);printf('d');printf(lonM);printf('m');printf(lonS);printf('s'); 
     printf("\n\rLongitude: %c %dd%dm%ds ",c,lonD,lonM,lonS); 
	//Charact ccc = c;//Integer llonD = lonD;//Integer llonM = lonM;//Integer llonS = lonS;
    // //contenu.append("Longitude: "+ccc.to//String()+" "+llonD.to//String()+"d"+llonM.to//String()+"m"+llonS.to//String()+"s\r\n");
    //contenu.append("Longitude: ").append(ccc.to//String()).append(" ").append(llonD.to//String()).append("d").append(llonM.to//String()).append("m").append(llonS.to//String()).append("s");
    double gpsLon= lonD+(60.0*lonM+lonS)/3600.0;
    printf(" = %3.6f Deg\n",gpsLon);
    //contenu.append(" = ").append(decform.format(gpsLon)).append(" Deg\r\n");
    GeogToUTM(gpsLat, gpsLon);
    /*if (s[110]==1) 
    {
     printf("Encoded Position Data Source Internal");
        //contenu.append("Encoded Position Data Source Internal\r\n");
    }
    else    
    {
    printf("Encoded Position Data Source External");
        //contenu.append("Encoded Position Data Source External\r\n");
    }
    s[109]=1;
    s[108]=0; 
    s[107]=1; 
    s[106]=1; */	   
  }
  
void identification_MMSI()
	{int i,b;
	float a=1,x=0;
	int xx;
	for(i=59;i>39;i--)
		{if (s[i]=='1') x+=a;
                a*=2;
		}
	xx=(int)x;
	printf("\n\rIdentifiant MMSI: ");
    //contenu.append("Identifiant MMSI: ");
    //Integer xxx=xx;
    printf("%d",xx);
    //contenu.append(xxx.to//String()).append("\r\n");
	i= 60;
	//b=8*(s[i]=='1')+4*(s[i+1]=='1')+2*(s[i+2]=='1')+(s[i+3]=='1');
	b=calcul(i,i+3);
    printf("\n\rBeacon : ");
    //contenu.append("Beacon : ");
    //Integer bb=b;
    printf("%d",b);
    //contenu.append(bb.to//String()).append("\r\n");
}

void identification_AIRCRAFT_24_BIT_ADRESS()
{
	int i,j,aa;
	float a=1,x=0;
	int xx;
	for(i=63;i>39;i--)
            {if (s[i]=='1') x+=a;
             a*=2;
            }
	xx=(int)x;
	printf("\n\rIdentifiant AIRCRAFT 24 BIT ADRESSE: ");
    //contenu.append("Identifiant AIRCRAFT 24 BIT ADRESSE: ");
    //Integer xxx=xx;
    printf("%d",xx);
    //contenu.append(xxx.to//String()).append("\r\n");     
    printf("en Hexa:");
    //contenu.append(" en Hexa: ");
    for(j=0;j<3;j++)
        {i=40+j*8;                              
         aa=calcul(i,i+7);
         envoi_byte(aa);
         } 
}

void identification_AIRCRAFT_OPER_DESIGNATOR()
{	int i,b;
	float a=1,x=0;
	int xx;
	for(i=54;i>39;i--)
	{if (s[i]=='1') x+=a;
         a*=2;
	}
	xx=(int)x;
	printf("\n\rIdentifiant AIRCRAFT OPER DESIGNATOR: ");
    //contenu.append("Identifiant AIRCRAFT OPER DESIGNATOR: ");
    printf("%d",xx);	
    //Integer xxx=xx;
    //contenu.append(xxx.to//String()).append("\r\n");
	i= 55;
	//b=256*(s[i]=='1')+128*(s[i+1]=='1')+64*(s[i+2]=='1')+32*(s[i+3]=='1')+ 16*(s[i+4]=='1')+8*(s[i+5]=='1')+4*(s[i+6]=='1')+2*(s[i+7]=='1')+(s[i+8]=='1');
	b=calcul(i,i+8);
    printf("\n\rSERIAL No: ");
    //contenu.append("SERIAL No: ");
    //Integer bb=b;
        printf("%d",b);
        //contenu.append(bb.to//String()).append("\r\n");
	}
    void identification_C_S_TA_No()
	{
     int i,b;
	 float a=1,x=0;
	 int xx;
	 for(i=49;i>39;i--)
		{if (s[i]=='1') x+=a;
                a*=2;
		}
	xx=(int)x;
	printf("\n\rIdentifiant C/S TA No: ");
        //contenu.append("Identifiant C/S TA No: ");
        //Integer xxx=xx;
        printf("%d",xx);	
        //contenu.append(xxx.to//String()).append("\r\n");
	i= 50;
	//b=8192*(s[i]=='1')+4096*(s[i+1]=='1')+2048*(s[i+2]=='1')+1024*(s[i+3]=='1')+512*(s[i+4]=='1')+256*(s[i+5]=='1')+128*(s[i+6]=='1')+64*(s[i+7]=='1')+32*(s[i+8]=='1')+ 16*(s[i+9]=='1')+8*(s[i+10]=='1')+4*(s[i+11]=='1')+2*(s[i+12]=='1')+(s[i+13]=='1');
	b=calcul(i,i+13);
        printf("\n\rSERIAL_No: ");
        //contenu.append("SERIAL_No: ");
        //Integer bb=b;
        printf("%d",b);
        //contenu.append(bb.to//String()).append("\r\n");
	}

void identification_MMSI_FIXED()
{
	int i,b;
	float a=1,x=0;
	int xx;
	for(i=59;i>39;i--)
            {if (s[i]=='1') x+=a;
                 a*=2;
            }
	xx=(int)x;
	printf("\n\rIdentifiant MMSI: ");
        //contenu.append("Identifiant MMSI: ");
        //Integer xxx=xx;
        printf("%d",xx);
        //contenu.append(xxx.to//String()).append("\r\n");
	i= 60;
	//b=8*(s[i]=='0')+4*(s[i+1]=='0')+2*(s[i+2]=='0')+(s[i+3]=='0');
	b=calcul(i,i+3);
        printf("Fixed: ");
        //contenu.append("Fixed: ");
        //Integer bb=b;
        printf("%d",b);
        //contenu.append(bb.to//String()).append("\r\n");
}



void localisation_nationale() //voir doc A-27-28-29
	{//printf("\n\rID et localisation Nationale à implanter");//contenu.append("ID et localisation Nationale à implanter\r\n");
	char c; int i,x,latD,latM,lonD,lonM,latS,lonS;
    int a;
	if (s[58]=='0') c='N'; else c='S';
	i=59;
	//latD=64*(s[i]=='1')+32*(s[i+1]=='1')+16*(s[i+2]=='1')+8*(s[i+3]=='1')+4*(s[i+4]=='1')+2*(s[i+5]=='1')+(s[i+6]=='1');
    latD=calcul(i,i+6);
    //latM=2*(16*(s[i+7]=='1')+8*(s[i+8]=='1')+4*(s[i+9]=='1')+2*(s[i+10]=='1')+(s[i+11]=='1'));
	latM=2*calcul(i+7,i+11);
    i=113;
	//x=2*(s[i]=='1')+(s[i+1]=='1');
	x=calcul(i,i+1);
    if(s[112]=='0') {x=-x;}
    latM+=x;
	i=115;
	//latS=4*(8*(s[i]=='1')+4*(s[i+1]=='1')+2*(s[i+2]=='1')+(s[i+3]=='1'));
	latS=4*calcul(i,i+3);
    if(s[112]=='0') //{latS=-latS;} 
				{latS=60-latS;latM--;
					if (latM<0) {latM=60+latM;latD--;}	
				}
	
	//printf("Latitude: ");printf(c);printf(' ');printf(latD);printf('d');printf(latM);printf('m');printf(latS);printf('s'); 
	printf("\n\rLatitude : %c %dd%dm%ds ",c,latD,latM,latS);
    //Charact cc = c;//Integer llatD = latD;//Integer llatM = latM;//Integer llatS = latS;
    //contenu.append("\r\nLatitude : ").append(cc.to//String()).append(" ").append(llatD.to//String()).append("d").append(llatM.to//String()).append("m").append(llatS.to//String()).append("s");
    double gpsLat= latD+(60.0*latM+latS)/3600.0;
    printf(" = %2.6f Deg",gpsLat);
    //contenu.append(" = ").append(decform.format(gpsLat)).append(" Deg\r\n");
    if (s[71]=='0') c='E'; else c='W';
	i=72;
	//lonD=128*(s[i]=='1')+64*(s[i+1]=='1')+32*(s[i+2]=='1')+16*(s[i+3]=='1')+8*(s[i+4]=='1')+4*(s[i+5]=='1')+2*(s[i+6]=='1')+(s[i+7]=='1');
    lonD=calcul(i,i+7);
    //lonM=2*(16*(s[i+8]=='1')+8*(s[i+9]=='1')+4*(s[i+10]=='1')+2*(s[i+11]=='1')+(s[i+12]=='1'));
	lonM=2*calcul(i+8,i+12);
    i=120;
	//x=2*(s[i]=='1')+(s[i+1]=='1');
	x=calcul(i,i+1);
    if(s[119]=='0') {x=-x;}
    lonM+=x;
	i=122;
	//lonS=4*(8*(s[i]=='1')+4*(s[i+1]=='1')+2*(s[i+2]=='1')+(s[i+3]=='1'));
	lonS=4*calcul(i,i+3);
    if(s[119]=='0')// {lonS=-lonS;}
	        	{lonS=60-lonS;lonM--;
					if (lonM<0) {lonM=60+lonM;lonD--;}	
				}
	
 
	//printf("Longitude: ");printf(c);printf(' ');printf(lonD);printf('d');printf(lonM);printf('m');printf(lonS);printf('s'); 
    printf("\n\rLongitude: %c %dd%dm%ds ",c,lonD,lonM,lonS); //Charact ccc = c;//Integer llonD = lonD;//Integer llonM = lonM;//Integer llonS = lonS;
    //contenu.append("Longitude: ").append(ccc.to//String()).append(" ").append(llonD.to//String()).append("d").append(llonM.to//String()).append("m").append(llonS.to//String()).append("s");
    double gpsLon= lonD+(60.0*lonM+lonS)/3600.0;
    printf(" = %3.6f Deg\n",gpsLon);
    //contenu.append(" = ").append(decform.format(gpsLon)).append(" Deg\r\n");
    GeogToUTM(gpsLat, gpsLon);
    i=126;
    //a=32*(s[i]=='0')+16*(s[i+1]=='0')+8*(s[i+2]=='0')+4*(s[i+3]=='0')+2*(s[i+4]=='0')+(s[i+5]=='0');
    a=calcul(i,i+5);
    printf("\n\rNational Use");
    //contenu.append("National Use\r\n");
  }
void identification_nationale()
	{int i;
	float a=1,x=0;
    int xx;
	for(i=57;i>39;i--)
		{if (s[i]=='1') x+=a;
                a*=2;
        }
	xx=(int)x;
	printf("\n\rIdentifiant National: ");
    //contenu.append("Identifiant National: ");
    //Integer xxx=xx;
    printf("%d",xx);	
    //contenu.append(xxx.to//String()).append("\r\n");
	}
void localisation_user()
	{char c; int i,latD,latM,lonD,lonM;
	if (s[107]=='0') c='N'; else c='S';
	i=108;
	//latD=64*(s[i]=='1')+32*(s[i+1]=='1')+16*(s[i+2]=='1')+8*(s[i+3]=='1')+4*(s[i+4]=='1')+2*(s[i+5]=='1')+(s[i+6]=='1');
    latD=calcul(i,i+6);
    //latM=4*(8*(s[i+7]=='1')+4*(s[i+8]=='1')+2*(s[i+9]=='1')+(s[i+10]=='1'));
	latM=4*calcul(i+7,i+10);
    //  printf("Latitude: ");printf(c);printf(' ');printf(latD);printf('d');printf(latM);printf('m');
	printf("\n\rLatitude : %c %dd%dm ",c,latD,latM);
    //Charact cc = c;//Integer llatD = latD;//Integer llatM = latM;
    //contenu.append("\r\nLatitude : ").append(cc.to//String()).append(" ").append(llatD.to//String()).append("d").append(llatM.to//String()).append("m");
    double gpsLat= latD+latM/60.0;
    printf(" = %2.4f Deg",gpsLat);
    //contenu.append(" = ").append(decform.format(gpsLat)).append(" Deg\r\n");
    if (s[119]=='0') {c='E';} else {c='W';}
	i=120;
	//lonD=128*(s[i]=='1')+64*(s[i+1]=='1')+32*(s[i+2]=='1')+16*(s[i+3]=='1')+8*(s[i+4]=='1')+4*(s[i+5]=='1')+2*(s[i+6]=='1')+(s[i+7]=='1');
    lonD=calcul(i,i+7);
    //lonM=4*(8*(s[i+8]=='1')+4*(s[i+9]=='1')+2*(s[i+10]=='1')+(s[i+11]=='1'));
	lonM=4*calcul(i+8,i+11);
    //printf("Longitude: ");printf(c);printf(' ');printf(lonD);printf('d');printf(lonM);printf('m'); 
	printf("\n\rLongitude: %c %dd%dm ",c,lonD,lonM); 
    //Charact ccc = c;//Integer llonD = lonD;//Integer llonM = lonM;
    //contenu.append("Longitude: ").append(ccc.to//String()).append(" ").append(llonD.to//String()).append("d").append(llonM.to//String()).append("m");
    double gpsLon= lonD+lonM/60.0;
    printf(" = %3.6f Deg",gpsLon);
    //contenu.append(" = ").append(decform.format(gpsLon)).append(" Deg\r\n");
    printf(" = %3.4f Deg\n",gpsLon);
    GeogToUTM(gpsLat, gpsLon);
    if (s[106]=='1') {printf("Encoded position data source internal");}
    //contenu.append("Encoded position data source internal\r\n");}
    }
void auxiliary_radio_locating_device_types()
    {int a;
  	//a=2*(s[83]=='1')+(s[84]=='1');
	a=calcul(83,84);
    switch(a){
      case 0 :printf("\n\rNo Auxiliary radio-locating device");
          //contenu.append("No Auxiliary radio-locating device\r\n");
      break;
      case 1 :printf("\n\r121.5MHz");
          //contenu.append("121.5MHz\r\n");
      break;
	  case 2 :printf("\n\rMaritime locating 9GHz SART");
              //contenu.append("Maritime locating 9GHz SART\r\n");
          break;
	  case 3 :printf("\n\rOther auxiliary radio-locating devices");
              //contenu.append("Other auxiliary radio-locating devices\r\n");
          break;
    }
     printf(" %d",a);
 }   
   
void Emergency_code_use()
{     
     int a;
     if (s[106]=='1')
    { 	printf("\n\rEmergency code flag :");
       ////contenu.append("\r\nEmergency code flag :");
       if (s[107]=='1')
    	{
    		printf("\n\rAutomatic and manual");
    	}
    
	//a=8*(s[108]=='1')+4*(s[109]=='1')+2*(s[110]=='1')+(s[111]=='1');
        a=calcul(108,111);
	switch(a)
	{
       case 1 :printf("Fire/explosion");
           //contenu.append("\r\nFire/explosion\r\n");
       break;
	   case 2 :printf("Flooding");
               //contenu.append("\r\nFlooding\r\n");
           break;
	   case 3 :printf("Collision");
               //contenu.append("\r\nCollision\r\n");
           break;
	   case 4 :printf("Grounding");
               //contenu.append("\r\nGrounding\r\n");
           break;
	   case 5 :printf("Listing, in danger of capsizing");//contenu.append("Listing, in danger of capsizing\r\n");
           break;
	   case 6 :printf("Sinking");
               //contenu.append("\r\nSinking\r\n");
           break;
       case 7 :printf("Disabled and adrift");
           //contenu.append("\r\nDisabled and adrift\r\n");
       break;
	   case 0 :printf("Unspecified distress");
               //contenu.append("\r\nUnspecified distress\r\n");
           break;
	   case 8 :printf("Abandoning ship");
               //contenu.append("\r\nAbandoning ship\r\n");
           break;
       case 10:printf("Abandoning ship");
           //contenu.append("\r\nAbandoning ship\r\n");
       break;
    }
  }
 
}
	 
void Non_Emergency_code_use_()
{
  
    // char a;

      if (s[106]=='1')
      { 
      printf("\n\rEmergency code flag: ");
          //contenu.append("\r\nEmergency code flag: ");
      }
       if (s[107]=='1') 
       {
       	 printf("\n\rAutomatic and manual");
           //contenu.append("Automatic and manual\r\n");
       }
       if (s[108]=='1') 
       {   
          printf("fire");
           //contenu.append("fire\r\n");
       } 
       if (s[109]=='1')
       { 
         printf("Medical help required");
           //contenu.append("Medical help required\r\n");
       } 
       if (s[110]=='1')
       { 
      //     printf("disabled");//contenu.append("disabled\r\n");   
       } 
       if (s[111]=='1')
       { 
           printf("Non specifié");
           //contenu.append("Non specifié\r\n");   
       } 
  }    
  




void Serial_Number_20_Bits()
{
  	    int j,e=1,a=0;
        printf("\n\rSerial Number: "); 
        //contenu.append("Serial Number: ");
        for(j=62;j>42;j--)
            {if(s[j]=='1')   a=a+e;
                e=e*2;
            }
        printf("%d",a);
        //Integer aa=a;
        //contenu.append(aa.to//String()).append("\r\n");
}

/*void Serial_Number_20_Bits()
{
  	     	int i,a;
            printf("\r\nSerial Number (2o bits) : "); 
            
            i=43;
            //a=524288*(s[i]=='1')+262144*(s[i+1]=='1')+131072*(s[i+2]=='1')+65536*(s[i+3]=='1')+32768*(s[i+4]=='1')+16384*(s[i+5]=='1')+8192*(s[i+6]=='1')+4096*(s[i+7]=='1')+2048*(s[i+8]=='1')+1024*(s[i+9]=='1')+512*(s[i+10]=='1')+256*(s[i+11]=='1')+128*(s[i+12]=='1')+64*(s[i+13]=='1')+32*(s[i+14]=='1')+16*(s[i+15]=='1')+8*(s[i+16]=='1')+4*(s[i+17]=='1')+2*(s[i+18]=='1')+(s[i+19]=='1');
            a=calcul(i,i+19);        
            printf(a);
}
*/
void all_0_or_nat_use() 
{
  	int i,a;
    printf("\n\rAll 0 or nat use: ");
    //contenu.append("All 0 or nat use: ");
    i=63;
    //a=512*(s[i]=='1')+256*(s[i+1]=='1')+128*(s[i+2]=='1')+64*(s[i+3]=='1')+32*(s[i+4]=='1')+16*(s[i+5]=='1')+8*(s[i+6]=='1')+4*(s[i+7]=='1')+2*(s[i+8]=='1')+(s[i+9]=='1');
    a=calcul(i,i+9);
    printf("%d",a);
    //Integer aa=a;
    //contenu.append(aa.to//String()).append("\r\n");
}       

void Aircraft_24_Bit_Adress()
{
            int i,j;
            int a;
            printf("\n\rAircraft 24 Bit Adresse: ");
            //contenu.append("Aircraft 24 Bit Adresse: "); 
             
            for(j=0;j<3;j++)
			{
            i=43+j*8;
            //a=128*(s[i]=='1')+64*(s[i+1]=='1')+32*(s[i+2]=='1')+16*(s[i+3]=='1')+8*(s[i+4]=='1')+4*(s[i+5]=='1')+2*(s[i+6]=='1')+(s[i+7]=='1');
            a=calcul(i,i+7);
            envoi_byte(a);
            printf("_");
            //contenu.append("_");
            }
            
      //printf("\r\n");
      //contenu.append("\r\n");
}

void Additional_ELT_No()
{   int i;
    int a;
    printf("\n\rAdditional ELT No: ");
    //contenu.append("Additional ELT No: ");
    i=67;   
    //a=32*(s[i]=='1')+16*(s[i+1]=='1')+8*(s[i+2]=='1')+4*(s[i+3]=='1')+2*(s[i+4]=='1')+(s[i+5]=='1');
    a=calcul(i,i+5);
    printf("%d",a);
    //Integer aa=a;
    //contenu.append(aa.to//String()).append("\r\n");
}

void Operator_3_Letter_Designator()
{  	        int i,j,b;
            int a;
            printf("\n\rOperator 3 Letter Designator : "); 
            //contenu.append("Operator 3 Letter Designator : ");
            for(j=0;j<2;j++)
			{
                 i=43+j*8;
                 //a=128*(s[i]=='1')+64*(s[i+1]=='1')+32*(s[i+2]=='1')+16*(s[i+3]=='1')+8*(s[i+4]=='1')+4*(s[i+5]=='1')+2*(s[i+6]=='1')+(s[i+7]=='1');
                a=calcul(i,i+7);	 
                envoi_byte(a);
            }
            i=59;
            //b=2*(s[i]=='1')+(s[i+1]=='1');
            b=calcul(i,i+1);
            envoi_byte(b);
            //printf("\r\n");
            //contenu.append("\r\n");
}

void Serial_Number()
{   int i,a;
    printf("\n\rSerial Number: ");
    //contenu.append("Serial Number: ");
    i=61; 
    //a=2048*(s[i]=='1')+1024*(s[i+1]=='1')+512*(s[i+2]=='1')+256*(s[i+3]=='1')+128*(s[i+4]=='1')+64*(s[i+5]=='1')+32*(s[i+6]=='1')+16*(s[i+7]=='1')+8*(s[i+8]=='1')+4*(s[i+9]=='1')+2*(s[i+10]=='1')+(s[i+11]=='1');
    a=calcul(i,i+11);
    //Integer aa=a;
    printf("%d",a);
    //contenu.append(aa.to//String()).append("\r\n");
}

 void C_S_Cert_No_or_Nat_Use() 
 {  int a;
    printf("\n\rC/S Number or National: ");
    //contenu.append("C/S Number or National: ");   
    // a=512*(s[73]=='1')+256*(s[74]=='1')+128*(s[75]=='1')+64*(s[76]=='1')+32*(s[77]=='1')+16*(s[78]=='1')+8*(s[79]=='1')+4*(s[80]=='1')+2*(s[81]=='1')+(s[82]=='1');
    a=calcul(73,82);
    printf("%d",a);
    //Integer aa=a;
    //contenu.append(aa.to//String()).append("\r\n");
}
void affiche_serial_user()
	{ int a,j,i;
	//a=4*(s[39]=='1')+2*(s[40]=='1')+(s[41]=='1');
	a=calcul(39,41);
    switch(a)
		{case 0 :printf("\n\rELT serial number: ");
            //contenu.append("ELT serial number: ");
            break;
		case 3 :printf("\n\rELT aircraft 24 bits adress: ");
            //contenu.append("ELT aircraft 24 bits adress: ");
            break;
		case 1 :printf("\n\rELT aircraft operator designator and serial number: ");
            //contenu.append("ELT aircraft operator designator and serial number: ");
            break;
		case 2 :printf("\n\rFloat free EPIRB + serial number : ");
            //contenu.append("Float free EPIRB + serial number: ");
            break;
		case 4 :printf("\n\rNon float free EPIRB + serial number: ");
            //contenu.append("Non float free EPIRB + serial number: ");
            break;
		case 6 :printf("\n\rPersonal Locator Beacon (PLB) + serial number: ");
            //contenu.append("Personal Locator Beacon (PLB) + serial number: ");
            break;
		}
		printf("  bits 44 à 73 en hexa : ");
        //contenu.append("  bits 44 à 73 en hexa : ");
		for(j=0;j<4;j++)
			{i=43+j*8;
			//a=128*(s[i]=='1')+64*(s[i+1]=='1')+32*(s[i+2]=='1')+16*(s[i+3]=='1')+8*(s[i+4]=='1')+4*(s[i+5]=='1')+2*(s[i+6]=='1')+(s[i+7]=='1');
			a=calcul(i,i+7);
            envoi_byte(a);
			}
        // printf("\r\n");
        //contenu.append("\r\n");
	}

void affiche_serial_user_1()
	{ int a;
	//a=4*(s[39]=='1')+2*(s[40]=='1')+(s[41]=='1');
	a=calcul(39,41);
        switch(a)
		{case 0 :printf("\n\rELT serial number :");
                 Serial_Number_20_Bits();all_0_or_nat_use();C_S_Cert_No_or_Nat_Use();break;
		case 3 :printf("\n\rELT aircraft ");
                Aircraft_24_Bit_Adress();Additional_ELT_No();C_S_Cert_No_or_Nat_Use();break;
		case 1 :printf("\n\rELT aircraft operator designator and serial number : " );
                Operator_3_Letter_Designator();Serial_Number();C_S_Cert_No_or_Nat_Use();break;
		case 2 :printf("\n\rfloat free EPIRB + serial number :");
                Serial_Number_20_Bits();all_0_or_nat_use();C_S_Cert_No_or_Nat_Use();break;
		case 4 :printf("\n\rnon float free EPIRB + serial number :");
                Serial_Number_20_Bits();all_0_or_nat_use();C_S_Cert_No_or_Nat_Use();break;
		case 6 :printf("\n\rPersonal Locator Beacon (PLB) + serial number :");
                Serial_Number_20_Bits();all_0_or_nat_use();C_S_Cert_No_or_Nat_Use();break;
		//case 5 :printf("spare :");break;
         //case 7 :printf(spare :");break;	 
		
       }
    switch(a)
		{case 5 :case 7 :
          printf("Spare: ");
          //contenu.append("Spare: \r\n");
          if(s[42]=='0')
          {          
            printf("Serial identification number is assigned nationally ");
              //contenu.append("Serial identification number is assigned nationally \r\n");
          }         
          else
          {
            printf("Identification data include the C/S type approval certificate number ");
            //contenu.append("Identification data include the C/S type approval certificate number\r\n"); 
          }
        }
}

void test_beacon_data()
{   int i,j;
    int a;
   	for(j=0;j<6;j++)
            {
             i=39+j*8;
            //a=128*(s[i]=='1')+64*(s[i+1]=='1')+32*(s[i+2]=='1')+16*(s[i+3]=='1')+8*(s[i+4]=='1')+4*(s[i+5]=='1')+2*(s[i+6]=='1')+(s[i+7]=='1');
            a=calcul(i,i+7);
            envoi_byte(a);
            }
    //     i=79;
    //a=32*(s[i]=='1')+16*(s[i+1]=='1')+8*(s[i+2]=='1')+4*(s[i+3]=='1')+2*(s[i+4]=='1')+(s[i+5]=='1');
    //      a=calcul(i,i+5);
    //Integer aa=a;
    //      printf("%d",a);
    //contenu.append(aa.to//String()).append("\r\n");
}  

void orbitography_data()
{
    int i,j;
    int a;
   	for(j=0;j<5;j++)
			{
            i=39+j*8;
			//a=128*(s[i]=='1')+64*(s[i+1]=='1')+32*(s[i+2]=='1')+16*(s[i+3]=='1')+8*(s[i+4]=='1')+4*(s[i+5]=='1')+2*(s[i+6]=='1')+(s[i+7]=='1');
			a=calcul(i,i+7);  
            envoi_byte(a);
			}
             i=79;
             //a=32*(s[i]=='1')+16*(s[i+1]=='1')+8*(s[i+2]=='1')+4*(s[i+3]=='1')+2*(s[i+4]=='1')+(s[i+5]=='1');
             a=calcul(i,i+5);
             //Integer aa=a;
             printf("%d",a);
             //contenu.append(aa.to//String()).append("\r\n");
}  

void national_use()
{
    int i,j;
    int a;
   	for(j=0;j<5;j++)
            {
            i=39+j*8;
	        //a=128*(s[i]=='1')+64*(s[i+1]=='1')+32*(s[i+2]=='1')+16*(s[i+3]=='1')+8*(s[i+4]=='1')+4*(s[i+5]=='1')+2*(s[i+6]=='1')+(s[i+7]=='1');
            a=calcul(i,i+7);	  
            envoi_byte(a);//envoi_byte(a);
            }
    i=79;
    //a=32*(s[i]=='1')+16*(s[i+1]=='1')+8*(s[i+2]=='1')+4*(s[i+3]=='1')+2*(s[i+4]=='1')+(s[i+5]=='1');
    a=calcul(i,i+5);
    //Integer aa=a;
    printf("%d",a); 
    //contenu.append(aa.to//String());
    i=106;
    //a=32*(s[i]=='1')+16*(s[i+1]=='1')+8*(s[i+2]=='1')+4*(s[i+3]=='1')+2*(s[i+4]=='1')+(s[i+5]=='1');
    a=calcul(i,i+5);
    //Integer bb=a;
    printf("%d",a);
    //contenu.append(bb.to//String()).append("\r\n");    
}

void national_use_1()
{
    int i,j;
    int a;
   	for(j=0;j<5;j++)
			{
            i=39+j*8;
			//a=128*(s[i]=='1')+64*(s[i+1]=='1')+32*(s[i+2]=='1')+16*(s[i+3]=='1')+8*(s[i+4]=='1')+4*(s[i+5]=='1')+2*(s[i+6]=='1')+(s[i+7]=='1');
            a=calcul(i,i+7);  
            envoi_byte(a);
			}
    i=79;
    //a=32*(s[i]=='1')+16*(s[i+1]=='1')+8*(s[i+2]=='1')+4*(s[i+3]=='1')+2*(s[i+4]=='1')+(s[i+5]=='1');
    a=calcul(i,i+5);
    //Integer aa=a;
    printf("%d",a); 
    //contenu.append(aa.to//String()).append("\r\n");
}

/*void BCH_1()
{
  
 int i,j;
     int a;
     printf("\r\n Encoded BCH 1 ");  //contenu.append("Encoded BCH 1 \r\n");  
   	for(j=0;j<2;j++)
			{
                        i=85+j*8;
			//a=128*(s[i]=='1')+64*(s[i+1]=='1')+32*(s[i+2]=='1')+16*(s[i+3]=='1')+8*(s[i+4]=='1')+4*(s[i+5]=='1')+2*(s[i+6]=='1')+(s[i+7]=='1');
			a=calcul(i,i+7);  
                        envoi_byte(a);
			}
             i=101;
             //a=16*(s[i]=='1')+8*(s[i+1]=='1')+4*(s[i+2]=='1')+2*(s[i+3]=='1')+(s[i+4]=='1');
            a=calcul(i,i+4); 
            envoi_byte(a);
 }*/


/*void affiche_serial_user2()
{
	 int a;
	
	//a=8*(s[39]=='1')+4*(s[40]=='1')+2*(s[41]=='1')+(s[42]=='1');
	a=calcul(39,42);
        switch(a)
	{
		case 10 :printf("\r\nserial identification number is assigned nationaly ");break;
		 case 11 :printf("\r\nidentification data include the C/S type approval certificate number");break;
         case 14 :printf("\r\nserial identification number is assigned nationaly ");break;
		 case 15 :printf("\r\nidentification data include the C/S type approval certificate number");break;
	}
}*/  
    
void decodage_LCD()
{   int b;
	int i,j;
    int a;    
    for(j=0;j<2;j++) //bits de 0 à 15 en octet
                    {i=j*8;
                    //a=128*(s[i]=='1')+64*(s[i+1]=='1')+32*(s[i+2]=='1')+16*(s[i+3]=='1')+8*(s[i+4]=='1')+4*(s[i+5]=='1')+2*(s[i+6]=='1')+(s[i+7]=='1');
                    a=calcul(i,i+7);
                   // envoi_byte(a);
                    }
                      
    if(s[16]=='1')            
                {
                //a=128*(s[16]=='1')+64*(s[17]=='1')+32*(s[18]=='0')+16*(s[19]=='1')+8*(s[20]=='0')+4*(s[21]=='0')+2*(s[22]=='0')+(s[23]=='0');
                a=calcul(16,23);
                printf("\r\nTrame de test...");
                //contenu.append("\r\nTEST...\r\n");
                }  
            else
                {
                //a=128*(s[16]=='0')+64*(s[17]=='0')+32*(s[18]=='1')+16*(s[19]=='0')+8*(s[20]=='1')+4*(s[21]=='1')+2*(s[22]=='1')+(s[23]=='1');
                a=calcul(16,23);
                printf("\n\rTrame d'alerte... ");
                //contenu.append("\r\nALERTE...\r\n");
                }  
            //strcpy(chaine,"");
            if (s[24]=='0')
                {printf("\r\nUser Protocole-Localisation courte");
                //contenu.append("User Protocole/Localisation courte\r\n");
	            //code protocole 37-39
                //a=4*(s[36]=='1')+2*(s[37]=='1')+(s[38]=='1');
				a=calcul(36,38);
                switch(a)
					{
						case 2 :
							printf("\n\rEPIRB  MMSI/Radio: ");
                            //contenu.append("EPIRB  MMSI/Radio: ");
                            affiche_baudot42();specific_beacon();Emergency_code_use();
                            break;
						case 6 :
							printf("\n\rEPIRB  Radio: ");
                            //contenu.append("EPIRB  Radio: ");
							affiche_baudot_1();specific_beacon();Non_Emergency_code_use_();
							break;
						case 1 :
							printf("\n\rELT Aviation: ");
                            //contenu.append("ELT Aviation: ");
							affiche_baudot_2();Non_Emergency_code_use_();
							break;
						case 3 :printf("\r\nSerial User : ");
							affiche_serial_user_1();Non_Emergency_code_use_();
							break;
						case 7 :
                            printf("\n\rTest User: ");
                            //contenu.append("\r\nTest User: ");
                            test_beacon_data();  
                            break;
						case 0 :
							printf("\n\rOrbitography: ");
                            //contenu.append("Orbitography: ");
                            orbitography_data();
                            break;
						case 4 :
                            printf("National: ");
                            //contenu.append("National: ");
							national_use();
							break;
						case 5 :
							printf("Spare ");
                            //contenu.append("Spare ");
                            break;
					}
					 							
                switch(a)
					{
						case 2: case 6: case 1: case 3: auxiliary_radio_locating_device_types();/*BCH_1();*/
                        break;
					}
                //b=512*(s[26]=='1')+256*(s[27]=='1')+128*(s[28]=='1')+64*(s[29]=='1')+32*(s[30]=='1')+16*(s[31]=='1')+8*(s[32]=='1')+4*(s[33]=='1')+2*(s[34]=='1')+(s[35]=='1');
                b=calcul(26,35);
                //Integer bb=b;
                printf("\n\rCode Pays : %d\n\r",b);
                //contenu.append("Code Pays:").append(bb.to//String());
					
			}
        else //Trame longue
               {printf("\n\rTrame longue 144 bits");
               //contenu.append("Trame longue 144 bits\r\n");//Messages long
	        	if (s[25]=='0')
                    {printf("\r\nProtocole Standard ou National de Localisation");
                    //code protocole 37-40
                    //a=8*(s[36]=='1').toint())+4*(s[37]=='1').toint()+2*(s[38]=='1')+(s[39]=='1');
                    a=calcul(36,39);
                    switch(a)
					{
						case 2 :printf("\n\rEPIRB MMSI");
                                //contenu.append("EPIRB MMSI\r\n");
                                break;
						case 3 :printf("\n\rELT Location");
                                //contenu.append("ELT Location\r\n");
                                break;
						case 4 :printf("\n\rELT serial");
                                //contenu.append("ELT serial\r\n");
                                break;
						case 5 :printf("\n\rELT aircraft");
                                //contenu.append("ELT aircraft\r\n");
                                break;
						case 6 :printf("\n\rEPIRB serial");
                                //contenu.append("EPIRB serial\r\n");
                                break;
						case 7 :printf("\n\rPLB serial");
                                //contenu.append("PLB serial\r\n");
                                break;
						case 12 :printf("\n\rShip Security");
                                //contenu.append("Ship Security\r\n");
                                break;
						case 8 :printf("\n\rELT National");
                                //contenu.append("ELT National\r\n");
                                break;
						case 9 :printf("\n\rSpare National");
                                //contenu.append("Spare National\r\n");
                                break;
						case 10 :printf("\n\rEPIRB National");
                                //contenu.append("EPIRB National\r\n");
                                break;
						case 11 :printf("\n\rPLB National");
                                //contenu.append("PLB National\r\n");
                                break;
						case 14 :printf("\n\rStandard Test: ");
                                //contenu.append("Standard Test: ");
                                standard_test();
                                break;
						case 15 :printf("\n\rNational Test");
                                //contenu.append("National Test ");break;
						case 0 :
						case 1: //printf("\n\rOrbitography");
                                //contenu.append("Orbitography\r\n");
                                break;
						case 13 :printf("\n\rSpare....");
                                //contenu.append("Spare....\r\n");
                                break;
					}
					switch(a)
                                {case 2:
                                            identification_MMSI();localisation_standard();supplementary_data();	break;
                                 case 3:
                                            identification_AIRCRAFT_24_BIT_ADRESS();localisation_standard();supplementary_data();break;
                                 case 4:	case 6 :case 7:
                                              identification_C_S_TA_No();localisation_standard();supplementary_data();break;
                                 case 5:
                                            identification_AIRCRAFT_OPER_DESIGNATOR();localisation_standard();supplementary_data();break;
                                 case 14: 
                                            localisation_standard(); break;
                                 case 12:
                                               localisation_standard();
                                               supplementary_data();
                                               identification_MMSI_FIXED();
                                                break;
                                 case 8:case 9: case 10: case 11: case 15: 
                                               localisation_nationale();identification_nationale();supplementary_data_1();break;
                                }
                    }
				else
				{
					printf("\n\nUser Protocole-Localisation");
                    //contenu.append("User Protocole/Localisation\r\n");	
                    //code protocole 37-39
					//a=4*(s[36]=='1')+2*(s[37]=='1')+(s[38]=='1');
					a=calcul(36,38);
                    switch(a)
                        {
						case 2 :printf("\n\rEPIR MMSI/Radio :");
                        //contenu.append("EPIR MMSI/Radio\r\n");
						affiche_baudot42();specific_beacon();break;
						case 6 :printf("\n\rEPIRB Radio..");
                        //contenu.append("EPIRB Radio..\r\n");
						affiche_baudot_1();specific_beacon();
						break;
						case 1 :printf("\n\rELT Aviation: ");
                        //contenu.append("ELT Aviation: ");
						affiche_baudot_2();
						break;
						case 3 :printf("\n\rSerial User: ");
                        //contenu.append("Serial User: \r\n");
						affiche_serial_user_1();	
                        break;
						case 7 :printf("\n\rTest User: ");
                        //contenu.append("Test User: \r\n");
                        test_beacon_data();  
                        break;
						case 0 :printf("\n\rOrbitography: ");
                        //contenu.append("Orbitography: \r\n");
                        orbitography_data();
                        break;
						case 4 :printf("\n\rNational use: ");
                        //contenu.append("National use: \r\n");
                        national_use_1();
						break;
						case 5 :printf("\n\rSpare");
                        //contenu.append("Spare\r\n");
                        break;
					 }
					 switch(a)
					{
						case 2: case 6: case 1: case 3: localisation_user();auxiliary_radio_locating_device_types(); break;
					}
							 
				}
                //b=512*(s[26]=='1')+256*(s[27]=='1')+128*(s[28]=='1')+64*(s[29]=='1')+32*(s[30]=='1')+16*(s[31]=='1')+8*(s[32]=='1')+4*(s[33]=='1')+2*(s[34]=='1')+(s[35]=='1');
                b=calcul(26,35);
                //Integer bb=b;
                printf("\n\rCode Pays : %d\n\r",b);
                //contenu.append("Code Pays : ").append(bb.to//String()).append("\r\n");
			}		
}
//DecimalFormat decform = new DecimalFormat("###.######"); 


int lit_header(FILE *fp) {
    char txt[4+1] = "";
    char dat[4]="";
    int byte, j=0;
    if (fread(txt, 1, 4, fp) < 4) return -1;
    if (strncmp(txt, "RIFF", 4)) return -1;
    if (fread(txt, 1, 4, fp) < 4) return -1;
    if (fread(txt, 1, 4, fp) < 4) return -1;
    if (strncmp(txt, "WAVE", 4)) return -1;
    while(1)//rechercher "fmt "
        {if ( (byte=fgetc(fp)) == EOF ) return -1;
        txt[j % 4] = byte;
        j++; if (j==4) j=0;
        if (strncmp(txt, "fmt ", 4) == 0) break;
        }
    if (fread(dat, 1, 4, fp) < 4) return -1;
    if (fread(dat, 1, 2, fp) < 2) return -1;
    if (fread(dat, 1, 2, fp) < 2) return -1;
    N_canaux = dat[0] + (dat[1] << 8);
    if (fread(dat, 1, 4, fp) < 4) return -1;
    memcpy(&f_ech, dat, 4); 
    if (fread(dat, 1, 4, fp) < 4) return -1;
    if (fread(dat, 1, 2, fp) < 2) return -1;
    if (fread(dat, 1, 2, fp) < 2) return -1;
    bits = dat[0] + (dat[1] << 8);
    while(1) //debut des donnees   
        {if ( (byte=fgetc(fp)) == EOF ) return -1;
        txt[j % 4] = byte;
        j++; if (j==4) j=0;
        if (strncmp(txt, "data",4) == 0) break;
        }
    if (fread(dat, 1, 4, fp) < 4) return -1;
    //printf( "f_ech: %d\n", f_ech);
    //printf( "bits       : %d\n", bits);
    //printf( "N_canaux   : %d\n", N_canaux);
    if ((bits != 8) && (bits != 16)) return -1;
    ech_par_bit = f_ech/bauds;
    //printf( "ech par bit: %.2f\n", ech_par_bit);
    //printf( "****Attente de Trames****");
    return 0;
}


int lit_ech(FILE *fp) {  // int 32 bit
    int byte,ech=0,i,s=0;     // EOF -> 0x1000000
   // static int Avg[10];
    //float ss=0;
    for (i = 0; i < N_canaux; i++) {//mono =0 
        byte = fgetc(fp);
        if (byte == EOF) return 1000000;
        if (i == canal_audio) ech = byte;
        if (bits == 16) 
        {   byte = fgetc(fp);
            if (byte == EOF) return 1000000;
            if (i == canal_audio) ech +=  byte << 8;
        }
    }
    if (bits ==  8)  s = (ech-128)*256;   // 8bit: 00..FF, centerpoint 0x80=128
    if (bits == 16)  s = (short)ech;
    n_ech++;
   // moyenne(s);
   return s;
}

int duree_entre_pics(FILE *fp, int *nbr) {
    int echantillon;
    float n;
    n = 0.0;
    if (seuil==seuilm){
        do{echantillon = lit_ech(fp);
            //printf("\n\r%d",echantillon);
            if (echantillon == 1000000) return 1;
            n++;
         } while (echantillon < seuilM);
         seuil=seuilM;
    }
    else{
        do{
          echantillon = lit_ech(fp);
          //printf("\n\r%d",echantillon);
          if (echantillon == 1000000) return 1;
          n++;  
        }while (echantillon > seuilm);
        seuil=seuilm;
    }

    //l = (float)n / ech_par_bit;
     if  (n>32700) n=32700;
     *nbr = (int) n;
    return 0;
}




void affiche_hexa(){
    int j,i,a;
    printf("\n\r");
    for(j=0;j<((longueur_trame-24)/8);j++){
        i=24+j*8;
        //a=128*(s[i]=='1')+64*(s[i+1]=='1')+32*(s[i+2]=='1')+16*(s[i+3]=='1')+8*(s[i+4]=='1')+4*(s[i+5]=='1')+2*(s[i+6]=='1')+(s[i+7]=='1');
        a=calcul(i,i+7);
        envoi_byte(a);
        //printf("%X",a);
    } 
    //printf("\n\r");
}
 

int test_crc1(){
int g[]= {1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1};
int div[22] ;
int i,j,ss=0;
//pour adrasec: crc à 0 accepté  désactivé
  //int zero=0;
  //for(i=85;i<106;i++) {if (s[i]=='1') zero++;}
//fin adrasec
i=24;
for(j=0;j<22;j++)
	{
        if (s[i+j]=='1'){div[j]=1;}
        else div[j]=0;
    	}
while(i<85)
	{for(j=0;j<22;j++)
		{div[j]=div[j] ^ g[j];
		}
	//décalage
	while ((div[0]==0) && (i<85))
		{for (j=0;j<21;j++)
			{div[j]=div[j+1];
			}
		if (i<84)
			{if(s[i+22]=='1')div[21]=1;
                        else div[21]=0;
			}
		i++;
		}
	}
for(j=0;j<22;j++)
		{ss+=div[j];
		}
if (ss==0)   printf("\n CRC_1 OK");
//else 
//    {if(zero==0) {printf("\n CRC_1 null?");ss=0;}
//    else printf("\n CRC_1 Mauvais. ATTENTION: il y a des erreurs ");
//    }
return ss;
}

int test_crc2(){
int g[]= {1, 0, 1, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1};
int div[13];
int i,j,ss=0;
//pour adrasec: crc à 0 accepté  désactivé
  //int zero=0;
  //for(i=132;i<144;i++) {if (s[i]=='1') zero++;}
//fin adrasec
i=106;
for(j=0;j<13;j++)
	{
         if (s[i+j]=='1'){div[j]=1;}
        else div[j]=0;
	}
while(i<132)
	{for(j=0;j<13;j++)
		{div[j]=div[j] ^ g[j];
		}
	//décalage
	while ((div[0]==0) && (i<132))
		{for (j=0;j<12;j++)
			{div[j]=div[j+1];
			}
		if (i<131)
			{if(s[i+13]=='1') div[12]=1;
                         else   div[12]=0;
                        }
		i++;
                }
	}	
for(j=0;j<13;j++)
		{ss+=div[j];
		}
if (ss==0)   printf("\n CRC_2 OK");
//else 
//    {if(zero==0) {printf("\n CRC_2 null? ");ss=0;}
//    else printf("\n CRC_2 Mauvais. ATTENTION: il y a des erreurs ");
//    }
return ss;
}
int test_trame()
{int  L=112;
   int crc1;
   int crc2=0; //112 ou 144
   crc1=test_crc1();
   if (s[24]=='1') {L=144;crc2=test_crc2();}
   if ((crc1!=0)||(crc2!=0)) L=0;
   return L;
}

int affiche_trame(){
    int r=0;
    s[longueur_trame]=0;
    if(test_crc1()==0){
    r=1;    
    if (s[24]=='1') test_crc2();   
    //printf("\n\rNouvelle trame binaire: ");
    //printf("\n\r%s",s);
    //test_crc1();
    //if (s[24]=='1') test_crc2();
    printf("\n\rContenu hexadécimal: ");
    affiche_hexa();
    decodage_LCD();
    }
    return(r);
}
void affiche_aide(){
            printf("\n/*---------Decodage sur PI de Balise 406 (V6.0 par F4EHY)-----------------*/");
            printf("\n/*OPTIONS:                                                                */");
            printf("\n/*--help aide                                                             */");
            printf("\n/*--osm  affichage sur une carte OpenStreetMap                            */");
            printf("\n/*--une_minute timeout de 55 secondes pour une utilisation avec scan406   */");
            printf("\n/*--M1 à --M10  Max pour le calcul le seuil minimum (defaut --M3)         */"); 
            printf("\n/*--2 à --100 coeff pour calculer le seuil Max/coeff (default --100)      */");
            printf("\n/*------------------------------------------------------------------------*/");
            printf("\n/*LANCEMENT DU PROGRAMME (exemples):                                      */");
            printf("\n/*sox -t alsa default -t wav - lowpass 3000 highpass 10  2>/dev/null | ./dec406V6        */");
            printf("\n/*sox -t alsa default -t wav - lowpass 3000 highpass 10  2>/dev/null | ./dec406V6 --osm  */");
            printf("\n/*------------------------------------------------------------------------*/");
            printf("\n/*SI 'sox' EST ABSENT:                                                    */");
            printf("\n/*sudo apt-get install sox                                                */");    
            printf("\n/*------------------------------------------------------------------------*/\n");
}
int main(int argc, char **argv) {
    FILE *fp;       //stdin ou fichier wav
    char *nom;      //nom du fichier en argument
//    int nbr;        //nombre d'ech entre 2 pics
//    int pos=0;      //numero du bit 0 à 143 ou 0 à 111
//    int Delta=0;    //tolerance Nbr ech entre pics 
    int Nb=0;       //durée du bit en ech
//    int Nb2=0;      //durée demi bit en ech
//    int bitsync=0;  // pour compter les bits de synchronisation  à 1 
//    int syncOK=0;   // passe à 1 lorsque du premier '0' après les 15 '1'
//    int paire=0;    // pour ne pas considerer les pics d'horloge comme des bits (Manchester)
    int depart=0;   // pour détecter le début de synchronisation porteuse non modulee == silence
    int echantillon;
    int numBit=0;
	char etat='*';
	int cpte = 0;
//	int valide = 0;// pour les 160ms
//	int cptblanc = 0;
//	int Nblanc,Nb15;
  //  int trame_OK=0;     //pour mise au point
    int synchro=0;
    //int longueur_trame=144;ou 112
    double Y1=0.0;
    double Y[242];//tableau de 2*Nb ech  = 2*48000/400=240
    double Ymoy=0.0;
    double Max = 10e3;
	double Min = -Max;
    double max;
	double min;
    double coeff=100;//8
    double seuil0 = Min/coeff;
	double seuil1 = Max/coeff;
 //   int fin=0;
    int quitter=0;
    int i,j,k,l;
    int Nb15;
    clock_t t1,t2;
    double dt;
    double clk_tck= CLOCKS_PER_SEC;
    fp=NULL;
    nom = argv[0];  //nom du programme 
    ++argv;
    while ((*argv) && (flux_wav)) {
        if   (strcmp(*argv, "--help") == 0){affiche_aide();return 0;}
        
        else if ( (strcmp(*argv, "--osm") == 0) ) opt_osm=1;
        else if ( (strcmp(*argv, "--2") == 0) ) coeff=2;
        else if ( (strcmp(*argv, "--3") == 0) ) coeff=3;
        else if ( (strcmp(*argv, "--4") == 0) ) coeff=4;
        else if ( (strcmp(*argv, "--5") == 0) ) coeff=5;
        else if ( (strcmp(*argv, "--10") == 0) ) coeff=10;
        else if ( (strcmp(*argv, "--20") == 0) ) coeff=20;
        else if ( (strcmp(*argv, "--30") == 0) ) coeff=30;
        else if ( (strcmp(*argv, "--40") == 0) ) coeff=40;
        else if ( (strcmp(*argv, "--50") == 0) ) coeff=50;
        else if ( (strcmp(*argv, "--60") == 0) ) coeff=60;
        else if ( (strcmp(*argv, "--70") == 0) ) coeff=70;
        else if ( (strcmp(*argv, "--80") == 0) ) coeff=80;
        else if ( (strcmp(*argv, "--90") == 0) ) coeff=90;
        else if ( (strcmp(*argv, "--100") == 0) ) coeff=100;
        else if ( (strcmp(*argv, "--M1") == 0) ) Max=10e1;
        else if ( (strcmp(*argv, "--M2") == 0) ) Max=10e2;
        else if ( (strcmp(*argv, "--M3") == 0) ) Max=10e3;
        else if ( (strcmp(*argv, "--M4") == 0) ) Max=10e4;
        else if ( (strcmp(*argv, "--M5") == 0) ) Max=10e5;
        else if ( (strcmp(*argv, "--M6") == 0) ) Max=10e6;
        else if ( (strcmp(*argv, "--M7") == 0) ) Max=10e7;
        else if ( (strcmp(*argv, "--M8") == 0) ) Max=10e8;
        else if ( (strcmp(*argv, "--M9") == 0) ) Max=10e9;
        else if ( (strcmp(*argv, "--M10") == 0) ) Max=10e10;
        else if ( (strcmp(*argv, "--une_minute") ==0) ) opt_minute=1;
        
        else {fp = fopen(*argv, "rb");
            if (fp == NULL) {printf( "%s Impossible d'ouvrir le fichier\n", *argv);return -1;
            }
            flux_wav = 0;       //fichier wav ok on n'utilise pas stdin
        }
        ++argv;
    }
    if (flux_wav)  fp = stdin;  // pas de fichier .wav utiliser le flux standard 'stdin'
      
    printf("\n****Attente de Trames****\n");
    lit_header(fp);
 //   pos = 0;                    
//    Nb2=ech_par_bit/2;
    Nb=ech_par_bit;
 //   Delta=round(0.2*Nb);
    depart=1;
 //   Nblanc=(int)((double)(0.04*(double)f_ech));
    //java a adapter......
    l=0;
    while (quitter==0){
      max = Max;min = -max; seuil0 = min/coeff; seuil1 = max/coeff;longueur_trame=144;
      cpte=0;k=0;etat='-';
      numBit=0;synchro=0;depart=0;
      for (i=0;i<145;i++) {s[i]='-';}
      for (i=0;i<242;i++) {Y[i]=0.0;};
      k=0;
      t1=clock();
	  while (numBit<longueur_trame){ //144 ou 112 si trame courte construire la trame
        if (opt_minute==1){//timeout pour utilisation avec scan406
            t2=clock();
            dt=((double)(t2-t1))/clk_tck;
          //  printf("dt= %lf \n ",dt);
            if (dt>55.0) {fprintf(stderr,"Plus de 55s\n");fclose(fp);return 0;}
        }
     
        //lire 1 ech et autocorrelation 2bits
        echantillon = lit_ech(fp); //printf("\n\r%d",echantillon);
        if (echantillon == 1000000) {
            quitter=1;fprintf(stderr,"Fin de lecture wav\n");return 0;
        }
        l++;
        k=(k+1)%(2*Nb); //numéro de l'échantillon dans un tableau Y de 2bit
        Y[k]=echantillon;Y1=0.0;Ymoy=0.0;
        for (i=0;i<2*Nb;i++)
            {Ymoy=Ymoy+Y[i];
            }
        Ymoy=Ymoy/(2*Nb);
        for (i=0;i<Nb;i++)  //Autocorélation
            {j=(k+i+Nb)%(2*Nb);
            Y1=Y1+(Y[(k+i)%(2*Nb)]-Ymoy)*(Y[j]-Ymoy);
            }
        if(Y1>max){max=Y1;seuil1=max/coeff;}
        if(Y1<min){min=Y1;seuil0=min/coeff;}
        //fin autocorélation
             
        if (synchro==0){//attendre synchro 15 '1'
                if (depart==0){//attendre premier front montant (depart_15 = 1 lorsque trouvé)
                    if (Y1>seuil1) {
                        depart=1;//1er front trouvé
                    }
                    cpte=0;
                } 
                else{//attendre front descendant en comptant les echs
                    cpte++;
                    if (Y1<seuil0){//front descendant trouvé tester les bits à 1 (de 10 à 20 ok)
                        Nb15=cpte/Nb;
                        if ((Nb15<16)&&(Nb15>11)){//syncho 15 trouvée
                            synchro=1;cpte=0;
                            for (i=0;i<15;i++) {
                                s[i]='1';numBit=15;etat='0';
                            }
                        }
                        else{//erreur synchro recommencer
                             cpte=0;depart=0;synchro=0;etat='-';numBit=0;
                            
                        }
                    }
                    
                    
                }
        
        }
        else{//synchro OK => Traitement
            cpte++;
            if (s[24]=='0') {
                    longueur_trame=112;
                    }
            if (Y1>seuil1){//front montant
                if (etat=='0')// si vient Y1 de changer compter les bits et les affecter
                                {etat='1';cpte-=Nb/2;
                                while ((cpte>0)&&(numBit<longueur_trame))
                                    {if (s[numBit-1]=='1') {s[numBit]='0';}
                                    else {s[numBit]='1';}
                                    numBit++;cpte-=Nb;
                                    }
                                cpte=0;
                                }//etat *
                                
            }
            else{ 
                if (Y1<seuil0) {//front descendant
                if(etat=='1') //si Y1 vient de changer
                                {etat='0';cpte-=Nb/2;
                                while ((cpte>0)&&(numBit<149))
                                    {s[numBit]=s[numBit-1];
                                    numBit++;cpte-=Nb;
                                    }
                                cpte=0;
                                }
                }
                else{//entre les deux niveaux... ignore...
                    
                }
                
            }   
        
        }//fin du else synchro OK traitement
    }  //fin while (numBit<longueur_trame)
    //printf("\n************Mini %d\tMaxi %d\tMoy%d\tf_ech%d********",Mini,Maxi,Moy,f_ech);
    if(affiche_trame()==1){
        fprintf(stderr,"TROUVE\n\n"); 
        if (opt_minute==1) {fclose(fp); return 0;   
        }
    }
    else{
        fprintf(stderr,"Erreur CRC\n\n");
        if (opt_minute==1) {fclose(fp);return 0;
        }
    }
    }
    printf( "\n");
    fclose(fp);
    return 0;
}

