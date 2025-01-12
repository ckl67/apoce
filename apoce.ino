/* =================================================================
	Pilote Arduino pour optimiser la gestion de consommation électrique
    Christian Klugesherz
    Date : 5 janvier 2025 --> Très grosse simplification 
	
    Le schéma de la carte se trouve dans le répertoire Board
    La simulation se trouve sur
    https://www.tinkercad.com/things/i7El4JjrINq-pilote-contacteur
    L'Arduino Nano est basée sur l'ATmega328

    Code sous github : https://github.com/ckl67/apoce

	ATTENTION:
	===========
		Il est impératif de configurer la partie "Configuration" ci-dessous !
		
    Principe
    =========
    Entrée :
      * Signal Jour/Nuit - J/N
      * Signal Soleil - SOL
    Sortie :
      * Signal pour piloter contacteur Chauffe Eau 1 - CA1
      * Signal pour piloter contacteur Chauffe Eau 2 - CA2
      * Signal pour piloter contacteur Chargement Voiture - CV
	Bouton :
	  * BoutonJN
	  * BoutonSOL
	  * BoutonAUTO
    Modes :
	  Dans le principe, un changement de mode, va re-initialiser le compteur d'armement

Les Modes disponibles :

      -------------------------------------------
      * Mode basculement Valeur des Tempos
      -------------------------------------------
      Un appui simultanément sur les 3 boutons  JNR / SOL / Auto permet de modifier 
	    la valeurs des tempos, entre  
	    * Mode Réel
		* Mode simulation
      
	  -------------------------------------------
      * Mode JN : Jour-Nuit --> Led : Bleue Allumée
      --------------------------------------------
	 Si ModeArm = 0
        Si signal J/N = 1
            Basculement entre pilotage "CA1" puis "CA2" 
	 Si ModeArm = 1 
        Si signal J/N = 1
            Basculement entre pilotage "CA1" puis "CA2" puis "CV"  
		
    Si signal J/N = 0
          Pas de pilotage
		  
      -------------------------------------------
      * Mode SOL : Soleil --> Led Orange Allumée
      --------------------------------------------
    Si ModeArm = 0
        Si signal SOL = 1
          Basculement entre pilotage "CA1" puis "CA2"
	  Si ModeArm = 1 
        Si signal SOL = 1
          Basculement entre pilotage "CA1" puis "CA2" puis "CV"
    Si SOL = 0
          Pas de pilotage

      -------------------------------------------
      * Mode Auto : Jour-Nuit + Soleil --> Led Rouge Allumée
      --------------------------------------------
 	 Si ModeArm = 0
        Si signal SOL = 1 || Signal J/N = 1
          Basculement entre pilotage "CA1" puis "CA2"
	Si ModeArm = 1 
        Si signal SOL = 1 || Signal J/N = 1
          Basculement entre pilotage "CA1" puis "CA2" puis "CV"

	Si signal J/N = 0 ET SOL = 0
          Pas de pilotage
	
      -------------------------------------------
      * Bouton Armement pressé 1X --> Led Blanche Clignotante
      --------------------------------------------
		Quelque soit le mode : ModeArm = 1
			--> Nous intégrons la voiture dans le cycle 
			--> Nous utilisons la variable : SwitchContactSelection
			définie : ArmDuration
        ArmDuration_Real = 12 heures
 
      -------------------------------------------
      * Bouton Armement pressé 2X --> Led Blanche Clignotante Rapide
      --------------------------------------------
			Quelque soit le mode : ModeArm = 2
				Sans courant de nuit, ni Soleil  : Basculement entre pilotage "CA1" puis "CA2" puis "V" sur une durée 
					définie : ArmDuration
            ArmDuration_Real = 12 heures

      -------------------------------------------
      * Bouton Forcage CA1 . CA2 . V 
      -------------------------------------------
      Un premier appui sur le bouton va 
        * Positionner le forcage, 
      un deuxième va 
        * Positionner le mode 


 ================================================================== */
// ===== PROTOTYPES ======

void ActiveRelay(int);    // Active le relais
void DeActiveRelay(int);  // Desactive le Relais

void WorkMode_JN();    // Mode Nuit en même temps
void WorkMode_SOL();   // Mode Soleil : Uniquement Chauffe Eau rotatif
void WorkMode_Auto();  // Mode Auto, qui après n heures sur m jours va passer k jours en mode JNR

// ===== DEFINE ======

// Carte ou Simulation
#define BoardTinkercad 0
#define BoardHardware 1

// Broches Entrées
// Les entrées sont protégée à travers un Optocoupleur
// Nous mettons également une résistance Pull-Down
// voir commentaire ci-sessous
#define InCurrentSOL 2  // Entrée Lecture Consigne Puissance Soleil atteint
#define InCurrentJN 3   // Entrée Lecture courant de Nuit

// Les boutons sont en Pull UP
// https://www.electrosoftcloud.com/en/arduino-pull-up-pull-down-resistors/
// Une résistance interne de 20kΩ est connectée au 5v interne Arduino
// Le bouton poussoir, qui vient après la résistance, est connecté sur l'entrée Arduino, et est relié par l'autre côté à la masse
//   * appui      --> va ramener la tension à l'entrée à la masse
//   * non appuié --> va maintenir la tension à 5V
// Grâce à cela, nous n'avons plus besoin de résistance pull-up externe que nous pourrons économiser sur notre circuit.
// Cette technique est utilisée en priorité pour éviter une entrée laissée en l’air qui peut avoir n’importe quelle valeur comprise entre 0 et 5 V
// https://www.locoduino.org/spip.php?article122
// https://forum.arduino.cc/t/opto-4n35-on-arduino-digital-input-no-signal/93625
// --> Etat haut inversé

#define ButModeJN 4    // Bouton Mode Jour Nuit
#define ButModeSOL 5   // Bouton Mode Soleil
#define ButModeAUTO 6  // Bouton Mode Auto
#define ButArm 7       // Bouton Armement

// Broches Sorties
#define LedArm A0  // Led Armement (Une Entrée/Sortie Analogique peut être utilisée en Digitale)

#define LedModeAUTO 11  // Led pour choix mode Auto
#define LedModeSOL 12   // Led pour choix Mode Soleil
#define LedModeJN 13    // Led pour choix Mode Jour/Nuit

#define OutCA1 8  // Sortie pour piloter contacteur Chauffe Eau 1
#define OutCA2 9  // Sortie pour piloter contacteur Chauffe Eau 2
#define OutV 10   // Sortie pour piloter contacteur Voiture

// Modes de fonctionnement
#define ModeJN 0    // Mode Jour-Nuit
#define ModeSOL 2   // Mode Soleil
#define ModeAUTO 4  // Mode Auto

// Position Bit POur le mode Forcer
// 		CA1	CA2	V	FR
// 		0	0	0	0
// 		0	0	1	1
// 		0	1	0	2
// 		0	1 	1	3
// 		1	0	0	4
// 		1	0	1	5
// 		1	1 	0	6
// 		1	1	1	7

#define BitForceCA1 0x3  // Force Mode CA1
#define BitForceCA2 0x2  // Force Mode CA2
#define BitForceV 0x1    // Force Mode Voiture

// Modes d'Armement
#define ModeNoARM 0      // Pas de mode Armement
#define ModeARMSimple 1  // Mode Armement Simple
#define ModeARMDouble 2  // Mode Aremement Double

// ----------------------------------------------------
// ----------------------------------------------------
//                  DEBUT - CONFIGURATIONS
// ----------------------------------------------------
// ----------------------------------------------------
// Type de Carte : Simulation TinkerPad ou Carte Réelle
//    Choix entre : BoardTinkercad / BoardHardware
//    --> La différence tient en l'activation des sorties relais qui se font en
//    * Signal haut pour la Simulation BoardTinkercad
//    * Signal bas pour la carte BoardHardware
#define BoardType BoardHardware

// MODE Debug  avec Sortie Série
#define Debug_Mode_Serie false

// ----------------------------------------------------
//                   FIN - CONFIGURATIONS
// ----------------------------------------------------

// Interval de clignottement LED
//   Valeur multiplicateur en (s)
#define LedIntervalSlow 1000ul * 1
#define LedIntervalFast 1000ul / 2

// Durée Armement Mode forcé
//   Valeur multiplicateur en (s)
//   --> 12 heures = (3600 * 10)
#define ArmDuration_Real 1000ul * (3600 * 12)
#define ArmDuration_Simul 1000ul * (20)

// Interval de basculement entre les contacteurs
//   Valeur multiplicateur en (s)
//   En référence aux 8 heures de courant de nuit
//   --> 30 minutes entre basculement (60 * 30)
#define SwitchContactInterval_Real 1000ul * (60 * 30)
#define SwitchContactInterval_Simul 1000ul * (3)

// Définit la durée d'interval représentant 1 Jour
// Défaut = 1000ul * (60 * 60 * 24) = 86400000ul
#define PeriodeJourInterval_Real 1000ul * (60 * 60 * 24)
#define PeriodeJourInterval_Simul 1000ul * (15)

// ======================================= VARIABLES  ===============================

// Variable Buttons
boolean ButModeJNwasUp;
boolean ButModeSOLwasUp;
boolean ButModeAUTOwasUp;
boolean ButArmwasUp;

// Variable Bouton : Vérifie si un bouton est pressé 2 fois
boolean ButSecondPush;

// Variable Bouton : Vérifie si un bouton Arm est pressé 2 fois
boolean ButArmSecondPush;

// Temps
unsigned long CurrentMillis;

// Tempos
boolean NormalTempoInterval;
unsigned long val_ArmDuration;
unsigned long val_SwitchContactInterval;

// Variable "temps" pour contôler clignottement de toutes les LEDs
unsigned long LedPreviousMillis;
unsigned long LedArmPreviousMillis;

// Variable clignottement Led
int LedInterval = LedIntervalSlow;
int LedArmInterval = LedIntervalSlow;

// Variable temps pour contrôler les contacteurs
unsigned long SwitchContactPreviousMillis;

// Variable temps pour contrôler bouton armement
unsigned long ArmVPreviousMillis;

// Variable de rotation pour balayer les contacteurs
//    0=CA1 , 1=CA2 , 2=Voiture
int SwitchContactSelection;

// Etat Clignottement des LEDs
int LedBlinkingState;  // LOW ou HIGH, Valeur de Clignottement de la Led

// Etat LED pour aArmement
int LedArmBlinkingState;  // LOW ou HIGH, Valeur de Clignottement de la Led

// Etat de L'armement.
boolean ArmTriggerStatus;
boolean ArmDoubleTriggerStatus;

// Variable Mode de fonctionnement
int Mode;
int ModeArm;
int ModeSaved;
int ValForceMode;

// ======================================= SETUP ===============================
void setup() {

  if (Debug_Mode_Serie)
    Serial.begin(9600);

  pinMode(InCurrentSOL, INPUT);
  pinMode(InCurrentJN, INPUT);

  pinMode(ButModeJN, INPUT_PULLUP);
  pinMode(ButModeAUTO, INPUT_PULLUP);
  pinMode(ButModeSOL, INPUT_PULLUP);
  pinMode(ButArm, INPUT_PULLUP);

  pinMode(LedModeSOL, OUTPUT);
  pinMode(LedModeAUTO, OUTPUT);
  pinMode(LedModeJN, OUTPUT);
  pinMode(LedArm, OUTPUT);

  pinMode(OutCA1, OUTPUT);
  pinMode(OutCA2, OUTPUT);
  pinMode(OutV, OUTPUT);

  digitalWrite(LedModeJN, LOW);
  digitalWrite(LedModeSOL, LOW);
  digitalWrite(LedModeAUTO, LOW);
  digitalWrite(LedArm, LOW);
  DeActiveRelay(OutCA1);
  DeActiveRelay(OutCA2);
  DeActiveRelay(OutV);

  // Temps
  CurrentMillis = 0;

  // Tempos
  if (BoardType == BoardTinkercad) {
    val_ArmDuration = ArmDuration_Simul;
    val_SwitchContactInterval = SwitchContactInterval_Simul;
    NormalTempoInterval = false;
  } else {
    val_ArmDuration = ArmDuration_Real;
    val_SwitchContactInterval = SwitchContactInterval_Real;
    NormalTempoInterval = true;
  }

  // Variable pour contrôler les contacteurs
  SwitchContactPreviousMillis = CurrentMillis;
  SwitchContactSelection = 0;

  // Variable pour Armement
  LedPreviousMillis = CurrentMillis;
  LedArmPreviousMillis = CurrentMillis;
  LedArmBlinkingState = LOW;

  // Variable pour led clignottement en fonctionnement Forcé
  LedBlinkingState = LOW;

  // Tempo d'armement
  ArmVPreviousMillis = CurrentMillis;

  // Variable Mode de fonctionnement
  Mode = ModeAUTO;
  ModeArm = ModeNoARM;
  ModeSaved = ModeAUTO;
  ValForceMode = 0;

  // Variable Button
  ButModeJNwasUp = true;
  ButModeAUTOwasUp = true;
  ButModeSOLwasUp = true;
  ButArmwasUp = true;

  ButSecondPush = false;
  ButArmSecondPush = false;
}

// ======================== LOOP ====================================
void loop() {

  // Il n'y a pas de problème à utiliser millis() pendant plusieurs années,
  // si on utilise toujours la formule qui compare la différence de 2 temps à un seuil.
  CurrentMillis = millis();

  // ----------- Compteur de Clignotement Led  ---------------------
  if (CurrentMillis - LedPreviousMillis >= LedInterval) {
    LedPreviousMillis = CurrentMillis;

    // if the LED is off turn it on
    if (LedBlinkingState == LOW) {
      LedBlinkingState = HIGH;
    } else {
      LedBlinkingState = LOW;
    }
    // Durée clignottement (Aucun intérêt de clignoter plus vite dans le cas d'un deuxième appui !!)
    LedInterval = LedIntervalSlow;
  }

  // ----------- Compteur de Clignotement Led Armement---------------------
  if (CurrentMillis - LedArmPreviousMillis >= LedArmInterval) {
    LedArmPreviousMillis = CurrentMillis;

    // if the LED is off turn it on and vice-versa:
    if (LedArmBlinkingState == LOW) {
      LedArmBlinkingState = HIGH;
    } else {
      LedArmBlinkingState = LOW;
    }
    // Durée clignottement
    if (ModeArm == ModeARMDouble)
      LedArmInterval = LedIntervalFast;
    else
      LedArmInterval = LedIntervalSlow;
  }

  // ----- Compteur Armement : Fin du compteur  ---------------------
  if (CurrentMillis - ArmVPreviousMillis >= val_ArmDuration) {
    ArmVPreviousMillis = CurrentMillis;

    ModeArm = ModeNoARM;
    ButArmSecondPush = false;
  }

  // ----- Compteur Switch entre CA1, CA2 et Voiture ---------------------
  if (CurrentMillis - SwitchContactPreviousMillis >= val_SwitchContactInterval) {
    SwitchContactPreviousMillis = CurrentMillis;
    SwitchContactSelection = SwitchContactSelection + 1;

    if (ModeArm == ModeNoARM) {
      if (SwitchContactSelection > 1)
        SwitchContactSelection = 0;
    } else {
      if (SwitchContactSelection > 2)
        SwitchContactSelection = 0;
    }
  }

  // ---- Lecture Etat Bouton
  // Attention nous sommes en mode PULLUP --> valeur actif = 0 !
  // Le mode INPUT_PULLUP est disponible sur les broches de l'Arduino.
  // Une résistance interne de 20kΩ est connectée au 5v interne Arduino
  // Le bouton poussoir, qui vient après la résistance, est connecté sur l'entrée Arduino, et est relié par l'autre côté à la masse
  //   * appui      --> va ramener la tension à l'entrée à la masse
  //   * non appuié --> va maintenir la tension à 5V
  // Grâce à cela, nous n'avons plus besoin de résistance pull-up externe que nous pourrons économiser sur notre circuit.

  boolean ButModeJNisUp = digitalRead(ButModeJN);
  boolean ButModeSOLisUp = digitalRead(ButModeSOL);
  boolean ButModeAUTOisUp = digitalRead(ButModeAUTO);
  boolean ButArmisUp = digitalRead(ButArm);

  // --------------------------
  // si bouton Mode JN pressé
  // --------------------------
  if (ButModeJNwasUp && !ButModeJNisUp) {
    delay(10);
    ButModeJNisUp = digitalRead(ButModeJN);
    if (!ButModeJNisUp) {

      Mode = ModeJN;

      if (Mode != ModeJN) {
        ButSecondPush = false;
      }

      if (ButSecondPush == false) {
        ButSecondPush = true;
        // Force
        ValForceMode = ValForceMode & (0X77 & BitForceCA1);
      } else {
        ButSecondPush = false;
        // Reset
        ValForceMode = ValForceMode & (0X00 & BitForceCA1);
      }
    }

    // Init Variables
    // On remet l'armement à zéro
    ModeArm = ModeNoARM;
    ArmTriggerStatus = false;
    ArmDoubleTriggerStatus = false;
    ButArmSecondPush = false;
    ArmVPreviousMillis = CurrentMillis;

    // On recommence sur CA1
    SwitchContactSelection = 0;
    SwitchContactPreviousMillis = CurrentMillis;

    LedPreviousMillis = CurrentMillis;

    DeActiveRelay(OutCA1);
    DeActiveRelay(OutCA2);
    DeActiveRelay(OutV);
    delay(10);
  }
  ButModeJNwasUp = ButModeJNisUp;  // true = bouton relaché  --> mémorise l'état


  // --------------------------
  // si bouton Mode Soleil pressé
  // --------------------------
  if (ButModeSOLwasUp && !ButModeSOLisUp) {
    delay(10);
    ButModeSOLisUp = digitalRead(ButModeSOL);

    if (!ButModeSOLisUp) {

      Mode = ModeSOL;

      if (Mode != ModeSOL) {
        ButSecondPush = false;
      }

      if (ButSecondPush == false) {
        ButSecondPush = true;
        // Force
        ValForceMode = ValForceMode & (0X77 & BitForceCA2);

      } else {
        ButSecondPush = false;
        // Reset
        ValForceMode = ValForceMode & (0X00 & BitForceCA2);
      }
    }

    // Init Variables
    // On remet l'armement à zéro
    ModeArm = ModeNoARM;

    ArmTriggerStatus = false;
    ArmDoubleTriggerStatus = false;
    SwitchContactSelection = 0;
    SwitchContactPreviousMillis = CurrentMillis;
    LedPreviousMillis = CurrentMillis;
    ArmVPreviousMillis = CurrentMillis;

    DeActiveRelay(OutCA1);
    DeActiveRelay(OutCA2);
    DeActiveRelay(OutV);
    delay(10);
  }
  ButModeSOLwasUp = ButModeSOLisUp;  // = true bouton relaché  --> mémorise l'état

  // --------------------------
  // si bouton Mode Auto pressé
  // --------------------------
  if (ButModeAUTOwasUp && !ButModeAUTOisUp) {
    delay(10);
    ButModeAUTOisUp = digitalRead(ButModeAUTO);
    if (!ButModeAUTOisUp) {

      Mode = ModeAUTO;

      if (Mode != ModeAUTO) {
        ButSecondPush = false;
      }

      if (ButSecondPush == false) {
        ButSecondPush = true;
        // Force
        ValForceMode = ValForceMode & (0X77 & BitForceV);
      } else {
        ButSecondPush = false;
        // Reset
        ValForceMode = ValForceMode & (0X00 & BitForceV);
      }
    }

    // Init Variables
    // On remet l'armement à zéro
    ModeArm = ModeNoARM;

    ArmTriggerStatus = false;
    ArmDoubleTriggerStatus = false;
    SwitchContactSelection = 0;
    SwitchContactPreviousMillis = CurrentMillis;
    LedPreviousMillis = CurrentMillis;
    ArmVPreviousMillis = CurrentMillis;
    ButArmSecondPush = false;

    DeActiveRelay(OutCA1);
    DeActiveRelay(OutCA2);
    DeActiveRelay(OutV);
    delay(10);
  }
  ButModeAUTOwasUp = ButModeAUTOisUp;  // = true bouton relaché  --> mémorise l'état

  // --------------------------
  // si Bouton ARM pressé
  // --------------------------
  if (ButArmwasUp && !ButArmisUp) {
    delay(10);
    ButArmisUp = digitalRead(ButArm);

    if (!ButArmisUp) {

      if (ButArmSecondPush == false) {
        ModeArm = ModeARMSimple;
        ButArmSecondPush = true;
      } else {
        ModeArm = ModeARMDouble;
        ButArmSecondPush = false;
      }
    }
    // Init Variables
    // On remet l'armement à zéro
    ArmVPreviousMillis = CurrentMillis;
  }
  ButArmwasUp = ButArmisUp;  // true = bouton relaché  --> mémorise l'état

  // -----------------------------------------------------------
  //  -------------------------- Automate  ---------------------
  // -----------------------------------------------------------
  switch (Mode) {
    case ModeJN:
      WorkMode_JN();
      break;

    case ModeSOL:
      WorkMode_SOL();
      break;

    case ModeAUTO:
      WorkMode_Auto();
      break;

    default:
      WorkMode_Auto();
      break;
  }

  if (Debug_Mode_Serie) {
    Serial.print("Md=");
    Serial.print(Mode);
    Serial.write("   ");

    Serial.print("Ma=");
    Serial.print(ModeArm);
    Serial.write("   ");

    Serial.println();
  }

  // Délai
  delay(100);
}

// ===================================================================================================
// ========================================= PROCEDURES ==============================================
// ===================================================================================================

void ActiveRelay(int pin) {
  if (BoardType == BoardHardware) {
    // Resitance Pull Up
    digitalWrite(pin, LOW);
  } else {
    digitalWrite(pin, HIGH);
  }
}

// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------

void DeActiveRelay(int pin) {
  if (BoardType == BoardHardware) {
    // Resitance Pull Up
    digitalWrite(pin, HIGH);
  } else {
    digitalWrite(pin, LOW);
  }
}

// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
void WorkMode_JN() {

  digitalWrite(LedModeJN, HIGH);
  digitalWrite(LedModeSOL, LOW);
  digitalWrite(LedModeAUTO, LOW);

  // Clignottement ou Non Led Armement
  if (ModeArm != ModeNoARM)
    digitalWrite(LedArm, LedArmBlinkingState);
  else
    digitalWrite(LedArm, LOW);

  // -------------------------------------
  if (ModeArm != ModeARMDouble) {
    if ((digitalRead(InCurrentJN) == HIGH)) {
      if (SwitchContactSelection == 0) {
        ActiveRelay(OutCA1);
        DeActiveRelay(OutCA2);
        DeActiveRelay(OutV);
      }
      if (SwitchContactSelection == 1) {
        DeActiveRelay(OutCA1);
        ActiveRelay(OutCA2);
        DeActiveRelay(OutV);
      }
      if (SwitchContactSelection == 2) {
        DeActiveRelay(OutCA1);
        DeActiveRelay(OutCA2);
        ActiveRelay(OutV);
      }
    } else {
      DeActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      DeActiveRelay(OutV);
    }
  } else {
    //ModeArm == ModeARMDouble
    if (SwitchContactSelection == 0) {
      ActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      DeActiveRelay(OutV);
    }
    if (SwitchContactSelection == 1) {
      DeActiveRelay(OutCA1);
      ActiveRelay(OutCA2);
      DeActiveRelay(OutV);
    }
    if (SwitchContactSelection == 2) {
      DeActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      ActiveRelay(OutV);
    }
  }
}

// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
void WorkMode_SOL() {

  digitalWrite(LedModeJN, LOW);
  digitalWrite(LedModeSOL, HIGH);
  digitalWrite(LedModeAUTO, LOW);

  // Clignottement ou Non Led Armement
  if (ModeArm != ModeNoARM)
    digitalWrite(LedArm, LedArmBlinkingState);
  else
    digitalWrite(LedArm, LOW);

  // -------------------------------------
  if (ModeArm != ModeARMDouble) {
    if ((digitalRead(InCurrentSOL) == HIGH)) {
      if (SwitchContactSelection == 0) {
        ActiveRelay(OutCA1);
        DeActiveRelay(OutCA2);
        DeActiveRelay(OutV);
      }
      if (SwitchContactSelection == 1) {
        DeActiveRelay(OutCA1);
        ActiveRelay(OutCA2);
        DeActiveRelay(OutV);
      }
      if (SwitchContactSelection == 2) {
        DeActiveRelay(OutCA1);
        DeActiveRelay(OutCA2);
        ActiveRelay(OutV);
      }
    } else {
      DeActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      DeActiveRelay(OutV);
    }
  } else {
    //ModeArm == ModeARMDouble
    if (SwitchContactSelection == 0) {
      ActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      DeActiveRelay(OutV);
    }
    if (SwitchContactSelection == 1) {
      DeActiveRelay(OutCA1);
      ActiveRelay(OutCA2);
      DeActiveRelay(OutV);
    }
    if (SwitchContactSelection == 2) {
      DeActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      ActiveRelay(OutV);
    }
  }
}

// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
void WorkMode_Auto() {

  digitalWrite(LedModeJN, LOW);
  digitalWrite(LedModeAUTO, HIGH);
  digitalWrite(LedModeSOL, LOW);

  // Clignottement ou Non Led Armement
  if (ModeArm != ModeNoARM)
    digitalWrite(LedArm, LedArmBlinkingState);
  else
    digitalWrite(LedArm, LOW);

  // -------------------------------------
  if (ModeArm != ModeARMDouble) {

    if ((digitalRead(InCurrentSOL) == HIGH) || (digitalRead(InCurrentJN) == HIGH)) {
      if (SwitchContactSelection == 0) {
        ActiveRelay(OutCA1);
        DeActiveRelay(OutCA2);
        DeActiveRelay(OutV);
      }
      if (SwitchContactSelection == 1) {
        DeActiveRelay(OutCA1);
        ActiveRelay(OutCA2);
        DeActiveRelay(OutV);
      }
      if (SwitchContactSelection == 2) {
        DeActiveRelay(OutCA1);
        DeActiveRelay(OutCA2);
        ActiveRelay(OutV);
      }
    } else {
      DeActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      DeActiveRelay(OutV);
    }
  } else {
    //ModeArm == ModeARMDouble
    if (SwitchContactSelection == 0) {
      ActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      DeActiveRelay(OutV);
    }
    if (SwitchContactSelection == 1) {
      DeActiveRelay(OutCA1);
      ActiveRelay(OutCA2);
      DeActiveRelay(OutV);
    }
    if (SwitchContactSelection == 2) {
      DeActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      ActiveRelay(OutV);
    }
  }
}

// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
