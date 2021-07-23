#include <Arduino.h>

#include "config.h"                                                                           //Configuration des librairies
#include "color.h"                                                                            //Couleur en HEX (COLOR_###)
#include "function.h"                                                                         //Vibrate() DisplayImage() SetBackLight() SetRTC() GetRTC()

//CONFIG//
#define SERIAL_SPEED 115200                                                                   //Vitesse de communication
//######//

//CLASS//
TTGOClass *ttgo;
TFT_eSPI *tft;
AXP20X_Class *power;
//#####//

unsigned int ss,mn,hh,dd,mm,yy; //Variables pour les dates et les heures
byte minuteCache = 99;          //Variable pour evite le refresh constant de l'heure
bool irq = false;               //Si on doit couper l'ecrant, le bouton (irq = interupt)
bool rtcIrq = false;            //Quand l'horloge envoie une interuption (alarme)
#define boutonPin AXP202_INT    //Simplification

//STATES//
bool wallpaperDisplayed;                                                                      //Si le fond d'ecran est déjà affiché
//######//

void setup(){
    Serial.begin(SERIAL_SPEED);                                                                 //On definie la vitesse de communication
    ttgo = TTGOClass::getWatch();                                                               //Récuperation des instance de la montre
  ttgo->begin();                                  //On initialise le materiel
    SetBackLight(true,255,ttgo);                                                                //On demarre le retroeclairage
  ttgo->motor_begin();                            //On démarre le moteur de vibration pin IO4
  tft = ttgo->tft;                              //Simplification
  power = ttgo->power;                          //Simplification
  
  //On met en route le moniteur de puissance
  power->adc1Enable(AXP202_VBUS_VOL_ADC1 | AXP202_VBUS_CUR_ADC1 | AXP202_BATT_CUR_ADC1 | AXP202_BATT_VOL_ADC1, true);

  pinMode(RTC_INT_PIN, INPUT_PULLUP);                     //On definie le rtc comme entrée
  attachInterrupt(RTC_INT_PIN, [] {rtcIrq = 1;}, FALLING);//Si l'horloge arrive à une alarme alors on passe la variable rtcIrq a True

  pinMode(boutonPin, INPUT_PULLUP);                     //On definie le bouton comme entrée
  attachInterrupt(boutonPin, [] {irq = true;}, FALLING);//Si le bouton est presser alors on passe la variable irq a True
  
  power->enableIRQ(AXP202_PEK_SHORTPRESS_IRQ, true);    //On met en place le systeme d'interuption grace au bouton
  power->clearIRQ();                                    //On vide l'interuption

  tft->setSwapBytes(1);                                 //Convertion pour l'affichage des image

    //DEFAULT//
    wallpaperDisplayed = false;                                                                 //Le fond d'ecran n'est pas encore affiché
    //#######//
 
  tft->fillScreen(TFT_BLACK);                           //On met l'ecrant en noir
  Serial.println("START");
    Vibrate(10,ttgo);                                                                           //On lance la vibration de demarrage
}

//Calculer l'heure et la date
void timeCalc(){
  //on récupère les données du RTC format (2019-08-12/15:00:56)
    String timeData = GetRTC();                                                                 //On recupere les données du RTC
  String date,times;                // Les variables des parties du timeData
  unsigned int pos = 0;             // La position des séparateurs

  //Separation des dates et des heures
  pos = timeData.indexOf("/");      //Recuperation de l'emplacement du séparateur
  date = timeData.substring(0,pos); //Recuperation de la partie des dates
  times = timeData.substring(pos+1,timeData.length());//Recuperation de la partie du temps

  //Separation des dates
  pos = date.indexOf("-");      //Recuperation de l'emplacement du séparateur
  yy = date.substring(0,pos).toInt();//Recuperation de la partie de l'annee
  pos = date.indexOf("-");      //Recuperation de l'emplacement du séparateur
  date = date.substring(pos+1,date.length());//Recuperation du reste de la partie
  mm = date.substring(0,pos).toInt();//Recuperation de la partie du mois
  dd = date.substring(pos+1,date.length()).toInt();//Recuperation de la partie des jours

  //Separation des heures
  pos = times.indexOf(":");                           //Recuperation de l'emplacement du séparateur
  hh = times.substring(0,pos).toInt();                //Recuperation de la partie des heures
  pos = times.indexOf(":");                           //Recuperation de l'emplacement du séparateur
  times = times.substring(pos+1,times.length());      //Recuperation du reste de la partie
  mn = times.substring(0,pos).toInt();                //Recuperation de la partie des minutes
  ss = times.substring(pos+1,times.length()).toInt(); //Recuperation de la partie des secondes
}

/*Barre des notification*/
void displayClock(){
  timeCalc();                             //Calcule de l'heure
  tft->setTextFont(2);                    //On met la taille de l'ecriture a 2
  tft->setTextColor(0xFFFF, TFT_BLACK);   //On choisie la couleur du texte a orange et le fond en noir
  tft->setCursor(0, 0);                   //On met le curseur en haut à gauche
  minuteCache = mn;                       //On remet le cache à jour
  
  /*Eviter les caratère parasite (réafficher un caratère vide)*/
  if (hh < 10){                                       //Si l'heure est sur 1 chiffre
    tft->print(" ");                                  //On affiche un caratère vide
  }
  tft->print(hh);                                     //On affiche l'heure
  tft->print(":");                                    //On affiche le séparateur heure/minute
  if (mn < 10){                                       //Si les minutes est sur 1 chiffre
    tft->print("0");                                  //On affiche un 0
  }
  tft->print(mn);                                     //On affiche les minute
}
void displayBatterie(){
  tft->setTextFont(2);                    //On met la taille de l'ecriture a 2
  tft->setCursor(200, 0);                 //On met le curseur en haut à gauche

  if (power->isChargeing()) {             //Si le chargeur est brancher
    tft->setTextColor(0x7E0, TFT_BLACK);  //On choisie la couleur du texte en vert et le fond en noir
  } else {
    if (power->getBattPercentage() < 20) {  //Si la batterie est inferieur à 20%
      tft->setTextColor(0xF800,TFT_BLACK);//On choisie la couleur du texte en rouge et le fond en noir
    } else {
      tft->setTextColor(0xFFFF,TFT_BLACK);//On choisie la couleur du texte en blanc et le fond en noir
    }
  }
  if (power->getBattPercentage() < 100) {  //Si la batterie n'a que deux chiffre
    tft->print(" ");                      //On affiche un espace
  }
  if (power->getBattPercentage() < 10) {  //Si la batterie n'a qu'un chiffre
    tft->print(" ");                      //On affiche un espace
  }
  tft->print(power->getBattPercentage()); //On affiche la batterie
  tft->println("%");                      //On affiche le logo%
}
void notificationBar(){
  //Rectangle en haut de l'ecran
  
  /*Affichage de l'heure*/
  if (minuteCache != mn) { //Si les minute on changer
    Serial.println("reload hour");
    displayClock();
  }

  /*Affichage de la batterie*/
  displayBatterie();
}

void loop(){
    if(!wallpaperDisplayed){                                                //Si le fond d'ecran n'est pas encore affiché
        DisplayImage(1,0,0,ttgo);                                               //Affichage de l'image de fond
        wallpaperDisplayed = true;                                              //Le fond d'ecran est affiché
    }

    notificationBar();                                                      //Affichage de la barre des notification

    
    /*Si on appuis sur l'ecran*/
    int16_t x, y;
    if (ttgo->getTouch(x, y)) {
        Serial.println("Change Mode");
        delay(100);
    }

    /*bouton pressé*/
    if (irq) {
        irq = false;                          //on remet la variable trigger à False
        power->readIRQ();                     //on regarde le type d'interuption
        if (power->isPEKShortPressIRQ()) {    //si l'interuption est une pression rapide
        //if (screenDisplay == true){         //Si l'ecran est allumer alors on l'eteint
            //Serial.println("Turn off screen");
            //screenDisplay = false;            //On eteint l'ecran
            //setVibrator(200);                 //On vibre pendant 0.2 seconde
        //}else{                              //Sinon l'ecran est eteind alors on l'allume
            //Serial.println("Turn on screen");
            //screenDisplay = true;             //On allume l'ecran
            //setVibrator(200);                 //On vibre pendant 0.2 seconde
        //}
        
        // Clean power chip irq status
        power->clearIRQ();
        // Set screen and touch to sleep mode
        ttgo->displaySleep();
        ttgo->powerOff();

        //Set all channel power off
        power->setPowerOutPut(AXP202_LDO3, false);
        power->setPowerOutPut(AXP202_LDO4, false);
        power->setPowerOutPut(AXP202_LDO2, false);
        power->setPowerOutPut(AXP202_EXTEN, false);
        power->setPowerOutPut(AXP202_DCDC2, false);
    
        // TOUCH SCREEN  Wakeup source
        //esp_sleep_enable_ext1_wakeup(GPIO_SEL_38, ESP_EXT1_WAKEUP_ALL_LOW);
        // PEK KEY  Wakeup source
        esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
        esp_deep_sleep_start();
        }
        power->clearIRQ();
        delay(1000);
    }
    
    delay(100);
}
