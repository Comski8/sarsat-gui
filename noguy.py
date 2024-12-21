import subprocess
import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

# Fonction pour écrire dans un log
def log_watchdog(message):
    with open("watchdog.log", "a") as logfile:
        logfile.write(message + "\n")
    print(message)  # Affiche également dans la console

# Fonction pour envoyer un e-mail en cas de crash
def send_email_on_crash(script_name, retry_attempts, max_retries, error_code, last_output):
    try:
        # Charger les informations de config_mail.txt
        config = {}
        with open("config_mail.txt", "r") as file:
            for line in file:
                key, value = line.strip().split("=")
                config[key.strip()] = value.strip()

        smtp_server, smtp_port = config["smtp_serveur"].split(":")
        sender_email = config["utilisateur"]
        sender_password = config["password"]
        recipient_email = config["destinataires"]

        # Configuration du message
        subject = f"Watchdog Alert: {script_name} a échoué après {retry_attempts} tentatives"
        body = (
            f"Le script {script_name} a planté après {retry_attempts} tentatives de relance.\n"
            f"Le Watchdog arrête maintenant les relances automatiques.\n\n"
            f"Code d'erreur : {error_code}\n"
            f"Dernière sortie du script :\n{last_output}\n\n"
            f"Veuillez vérifier le script ou l'environnement pour corriger le problème."
        )

        message = MIMEMultipart()
        message["From"] = sender_email
        message["To"] = recipient_email
        message["Subject"] = subject
        message.attach(MIMEText(body, "plain"))

        # Connexion au serveur SMTP
        with smtplib.SMTP(smtp_server, int(smtp_port)) as server:
            server.starttls()
            server.login(sender_email, sender_password)
            server.sendmail(sender_email, recipient_email, message.as_string())
            log_watchdog("E-mail d'alerte envoyé avec succès.")
    except Exception as e:
        log_watchdog(f"Erreur lors de l'envoi de l'e-mail : {e}")

# Fonction pour lancer un script et surveiller son état
def run_and_watch(command, script_name):
    max_retries = 3  # Nombre maximum de tentatives de relance
    retry_attempts = 0

    while retry_attempts < max_retries:
        log_watchdog(f"Lancement de la commande : {command} (Tentative {retry_attempts + 1}/{max_retries})")
        try:
            # Lancer le processus avec capture des sorties
            process = subprocess.Popen(
                command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
            )
            stdout, stderr = process.communicate()  # Capture les sorties
            error_code = process.returncode

            if error_code != 0:  # Si le processus s'arrête avec une erreur
                retry_attempts += 1
                log_watchdog(f"{script_name} s'est arrêté de manière inattendue avec le code d'erreur {error_code}.")
                log_watchdog(f"Dernière sortie : {stderr.strip() or stdout.strip()}")

                if retry_attempts >= max_retries:
                    log_watchdog(f"{script_name} a échoué après {max_retries} tentatives. Arrêt des relances.")
                    # Envoyer un e-mail avec les détails de l'échec
                    send_email_on_crash(
                        script_name,
                        retry_attempts,
                        max_retries,
                        error_code,
                        stderr.strip() or stdout.strip()  # Dernière sortie
                    )
                    break
            else:
                log_watchdog(f"{script_name} s'est terminé normalement.")
                break  # Sort de la boucle si le script se termine correctement
        except Exception as e:
            log_watchdog(f"Erreur inattendue lors de l'exécution de {script_name} : {e}")
            send_email_on_crash(script_name, retry_attempts + 1, max_retries, "Erreur inconnue", str(e))
            break

# Menu principal
def main():
    print("Quel méthode souhaitez-vous utiliser ?")
    print("1. Discri")
    print("2. SDR")
    choix = input("Entrez votre choix (1 ou 2) : ")

    if choix == "1":
        command = "./decode_MIC_email_406.pl"
        script_name = "decode_MIC_email_406.pl"
    elif choix == "2":
        command = "./scan406.pl 406M 406.1M"
        script_name = "scan406.pl"
    else:
        print("Choix invalide.")
        return  # Arrête le programme si le choix est invalide

    print("Souhaitez-vous relancer le script automatiquement en cas de crash ?")
    print("1. Oui")
    print("2. Non")
    relancer = input("Entrez votre choix (1 ou 2) : ")

    if relancer == "1":
        print("Mode Watchdog activé.")
        log_watchdog(f"Watchdog activé pour le script : {script_name}")
        run_and_watch(command, script_name)
    elif relancer == "2":
        print("Exécution unique.")
        subprocess.run(command, shell=True)
    else:
        print("Choix invalide.")

if __name__ == "__main__":
    # Nettoyage du log précédent
    with open("watchdog.log", "w") as logfile:
        logfile.write("Démarrage du Watchdog\n")
    main()
