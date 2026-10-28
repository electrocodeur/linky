<p align="center">
  <img src="PCB%20logo_PNG.png" alt="Logo PCB" width="300">
</p>

<p align="center">
  <a href="https://github.com/electrocodeur">
    <img src="https://github.com/electrocodeur.png" alt="Mon profil GitHub" width="120">
  </a>
</p>

# Module Linky → Home Assistant (MQTT)

Ce projet a pour objectif de réaliser un module permettant de se connecter directement au compteur **Linky** afin de récupérer les données de télé-information et de les transmettre à un serveur **Home Assistant** via **MQTT**.

Le but est de proposer une solution simple, fiable et facilement intégrable dans une installation domotique.

Le module est également disponible à l'achat pour les personnes qui souhaitent profiter de cette solution sans avoir à fabriquer elles-mêmes le circuit électronique.

➡️ **Acheter le module :** https://www.leboncoin.fr/ad/telephones_objets_connectes/3296448571

---

## Fonctionnalités

- 📡 Connexion au compteur **Linky** via la Télé-information Client (TIC).
- 🏠 Envoi des données vers **Home Assistant** grâce au protocole **MQTT**.
- ⚡ Suivi de la consommation électrique en temps réel.
- 🔧 Plusieurs versions du code disponibles pour s'adapter à votre installation.

---

## Choisir la bonne version du code

⚠️ **Plusieurs versions du code sont disponibles. Il est important de sélectionner celle qui correspond à votre compteur Linky et à votre type d'abonnement électrique.**

Avant de téléverser le code sur votre module, vérifiez les deux points suivants.

### 1. Identifier le type de compteur Linky

Votre compteur peut fonctionner selon deux modes de télé-information :

- **Mode standard** : généralement utilisé avec les installations Linky récentes et offrant davantage de données.
- **Mode historique** : mode de télé-information utilisé avec certaines installations ou configurations plus anciennes.

Vous devez sélectionner la version du code correspondant au mode de télé-information configuré sur votre compteur.

### 2. Identifier votre type d'abonnement

Choisissez ensuite la version adaptée à votre contrat d'électricité :

- **Option Base** : le prix du kWh est identique quelle que soit l'heure de consommation.
- **Option Heures Pleines / Heures Creuses (HP/HC)** : le prix du kWh varie selon la période de la journée. Le code doit donc correspondre à cette option pour exploiter correctement les index de consommation.

### 3. Sélectionner la version correspondante

Vous devez tenir compte de deux critères : le mode de télé-information de votre compteur et votre option tarifaire.

| Type de télé-information | Abonnement | Version du code à choisir |
|---|---|---|
| Standard | Base | Version Standard – Base |
| Standard | Heures Pleines / Heures Creuses | Version Standard – HP/HC |
| Historique | Base | Version Historique – Base |
| Historique | Heures Pleines / Heures Creuses | Version Historique – HP/HC |

**En résumé :** identifiez le mode de télé-information de votre compteur, vérifiez votre option tarifaire, puis sélectionnez la version du code correspondant à ces deux critères.

Un mauvais choix peut entraîner une lecture incorrecte de certaines données ou des index de consommation.

---

## Présentation vidéo

Une vidéo présentant le projet est disponible sur YouTube :

➡️ **https://youtu.be/GOWZHOOZNiU**

---

# ⚠️ Avertissement (Disclaimer)

Ce projet est partagé afin de permettre à d'autres passionnés de découvrir son fonctionnement.

Je **ne suis pas un professionnel** de l'électronique, de l'électricité ou des systèmes de comptage. Je suis simplement un passionné qui partage son travail.

En utilisant ce projet, vous acceptez que :

- vous réalisez le montage, les soudures et les branchements sous votre entière responsabilité ;
- vous êtes seul responsable des modifications apportées à votre installation ;
- je ne pourrai être tenu responsable d'un mauvais branchement sur votre compteur Linky ;
- je décline toute responsabilité en cas de dommage matériel, de dysfonctionnement du compteur, de perte de données ou de tout autre incident lié à l'utilisation de ce projet ;
- aucune garantie n'est fournie concernant le bon fonctionnement du matériel ou du logiciel.

---

# Fabrication du PCB

Découvrez la fabrication de PCB simple, économique et fiable avec **JLCPCB** !

Profitez dès aujourd'hui des coupons d'une valeur de **123 $** réservés aux nouveaux clients et bénéficiez également d'une offre spéciale : recevez un coupon supplémentaire de **16 $** valable sur les **PCB Premium 6 couches** de JLCPCB !

➡️ https://jlcpcb.com/fr?from=electrocodeur

---
## Licence

Ce projet est partagé librement afin de permettre à chacun d'apprendre et de reproduire le montage.
