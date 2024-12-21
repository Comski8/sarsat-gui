# Sarsat-GUI

Un décodeur SARSAT 406 avec interface graphique (GUI) basé sur le décodeur de [F4EHY](http://jgsenlis.free.fr).

## Description

Sarsat-GUI est une application qui regroupe plusieurs méthodes de décodage SARSAT :
- **Décodage par SDR** (utilise un dongle RTL-SDR).
- **Décodage par entrée micro** (discriminateur audio).
- **Décodage de fichiers WAV** (pour tests ou décodages ultérieurs).

### Fonctionnalités
- Les trames SARSAT décodées peuvent être automatiquement envoyées par e-mail après configuration dans `config_mail.txt`.
- Les e-mails transmettent désormais **la totalité de la trame décodée**.
- Ajout d'une version sans interface graphique (nogui.py) intégrant un watchdog avec relance automatique alerte par e-mail en cas de détection d'anomalie.
- Ce projet est en cours de développement. Certaines fonctions n'ont pas encore été testées et pourraient ne pas fonctionner correctement.

---

## Installation

### Étapes pour installer les dépendances :
1. Mettez à jour et installez les paquets requis :
   ```bash
   sudo apt update && sudo apt upgrade -y
   sudo apt install git rtl-sdr sox perl sendemail libxcb-xinerama0 python3-pyqt5
2. Clonez le dépôt et placez-vous dans le dossier :
   ```bash
   git clone https://github.com/Comski8/Sarsat-GUI.git
   cd Sarsat-GUI-main
3. Donnez les permissions d'exécution :
   ```bash
   chmod +x *.pl *.sh
   
---

## Utilisation

1. Executer : 
   ```bash
   python3 gui.py

---

## Remarques 

- Ce projet est un travail en cours. Certaines fonctionnalités pourraient ne pas être complètement fonctionnelles.

---

## Remerciements

Ce projet s'appuie sur le travail de [F4EHY](http://jgsenlis.free.fr), dont le décodeur SARSAT constitue la base. Et ChatGPT, parceque je suis pas doué.. mais j'ai l'envie :sweat_smile:






