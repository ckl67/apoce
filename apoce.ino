/* =================================================================
	Pilote Arduino pour optimiser la gestion de consommation électrique
    Christian Klugesherz
    Date : 25 janvier 2024
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
      * En prévision Signal pour piloter contacteur Chargement - X
    Modes :
		  Dans le principe, un changement de mode, va re-initialiser le compteur d'armement
		
	Les Modes disponibles :

      -------------------------------------------
      * Mode basculement Valeur des Tempos
      -------------------------------------------
      Un appui simultanément sur les 3 boutons  JNR / SolCA / SolCAVR permet de modifier 
	    la valeurs des tempos, entre  
	    * Mode Réel
		  * Mode simulation
      
	  -------------------------------------------
      * Mode JN : Jour-Nuit --> Led : Bleue Allumée
      --------------------------------------------
	 Si ModeArm = 0
        Si signal J/N = 1
          Basculement entre pilotage "CA1 + CA2" puis "CV"
        Si signal J/N = 0
          Pas de pilotage
	 Si ModeArm = 1x
        Si signal J/N = 1
          Basculement entre pilotage "CA1 + CA2 + CV" puis "CV"
        Si signal J/N = 0
          "CV"
	 Si ModeArm = 2x
          Basculement entre pilotage "CA1 + CA2" puis "CA2 + CV"
		  
      -------------------------------------------
      * Mode JNR : Jour-Nuit Rotatif --> Led : Bleue Clignotante
      --------------------------------------------
  	 Si ModeArm = 0
        Si signal J/N = 1
          Basculement entre pilotage "CA1" puis "CA2" puis "CV"
        Si signal J/N = 0
          Pas de pilotage
	 Si ModeArm = 1x
        Si signal J/N = 1
          Basculement entre pilotage "CA1+CV" puis "CA2+CV" puis "CV"
        Si signal J/N = 0
          "CV"
	 Si ModeArm = 2x
          Basculement entre pilotage "CA1" puis "CA2" puis "CV"

      -------------------------------------------
      * Mode SolCA : Soleil Chauffe Eau --> Led Orange Allumée
      --------------------------------------------
 	 Si ModeArm = 0
        Si signal SOL = 1
          Basculement entre pilotage "CA1" puis "CA2"
        Si signal J/N = 1
          Activation CV 
		    Si signal J/N = 0 ou SOL = 0
          Pas de pilotage
 	 Si ModeArm = 1x
        Si signal SOL = 1
          Basculement entre pilotage "CA1+CV" puis "CA2+CV"
 	 Si ModeArm = 2x
          Basculement entre pilotage "CA1 + CV" puis "CA2 + CV"
	
      -------------------------------------------
      * Mode SolCAVR : Soleil Rotatif --> Led Orange Clignotante
      --------------------------------------------
 	    Si ModeArm = 0
        Si signal SOL = 1
          Basculement entre pilotage "CA1" puis "CA2" puis "V"
        Si SOL = 0
          Pas de pilotage
 	    Si ModeArm = 1x
        Si signal SOL = 1
          Basculement entre pilotage "CA1+CV" puis "CA2+CV" puis "CV"
 	    Si ModeArm = 2x
          Basculement entre pilotage "CA1" puis "CA2" puis "CV"	

      -------------------------------------------
      * Mode Auto : Mode SolCAVR avec repliement vers JNR 
		  --> Led Orange Clignotante
		  --> Led Rouge : Fixe
      --------------------------------------------
		    Même chose que Mode SolCAVR
		    Si après NbPeriodeJour, il a y eu moins de Soleil que "QuotaMiniHeureSoleil"
		    Alors, nous nous replions vers le Mode JNR et on y reste

      -------------------------------------------
      * Mode AutoR : Mode SolCAVR avec repliement vers JNR et retour vers SolCAVR possible 
		  --> Led Orange Clignotante
		  --> Led Rouge : Clignatante
      --------------------------------------------
		    Même chose que Mode SolCAVR
		    Si après NbPeriodeJour, il a y eu moins de Soleil que "QuotaMiniHeureSoleil"
		    Alors, Nous nous replions vers le Mode JNR 
		    Si après NbPeriodeJour, il a y plus de Soleil que "QuotaMiniHeureSoleil"
		    Alors, Nous revenons vers le Mode SolCAVR 

      -------------------------------------------
      * Bouton Armement pressé 1X --> Led Blanche Clignotante
      --------------------------------------------
			Quelque soit le mode
				--> Nous forçons le chargement de la voiture sur une durée 
					définie : ArmVDuration
 
      -------------------------------------------
      * Bouton Armement pressé 2X --> Led Blanche Clignotante Rapide
      --------------------------------------------
			Quelque soit le mode, 
				--> Nous émulons le mode JNR (sans courant de nuit) sur une durée 
					définie : ArmVDuration

 ================================================================== */
// ===== PROTOTYPES ======

void ActiveRelay(int);    // Active le relay
void DeActiveRelay(int);  // Desactive le Relay

void WorkMode_JN();          // Mode Nuit en même temps
void WorkMode_JNR(int);      // Mode Nuit en Rotatif
void WorkMode_SolCA();       // Mode Soleil : Uniquement Chauffe Eau rotatif
void WorkMode_SolCAVR(int);  // Mode Soleil en Rotatif
void WorkMode_Auto();        // Mode Auto, qui après n heures sur m jours va passer k jours en mode JNR
void WorkMode_AutoR();       // Mode Auto, qui après n heures sur m jours va passer k jours en mode JNR, puis revenir en mode Auto

void WorkMode_DynChangeTempo();  // Mode de changement dynamique de la valeur des tempos

// ===== DEFINE ======

// Carte ou Simulation
#define BoardTinkercad 0
#define BoardHardware 1

// Broches Entrées
// Les entrées sont protégée à travers un Optocoupleur
// Nous mettons également une résistance Pull-Down
// voir commentaire ci-sessous
#define InCurrentSol 2  // Entrée Lecture Consigne Puissance Soleil atteint
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

#define ButModeJN 4       // Bouton Mode Jour Nuit
#define ButModeSolCAV 5   // Bouton Mode Soleil Chauffe Eau et Voiture
#define ButModeSolAuto 6  // Bouton Mode Soleil Auto
#define ButArm 7          // Bouton Armement

// Broches Sorties
#define LedArm A0          // Led Armement (Une Entrée/Sortie Analogique peut être utilisée en Digitale)
#define LedModeSolAuto 11  // Led pour choix : Soleil - mode Auto
#define LedModeSolCAV 12   // Led pour choix : Soleil Chauffage + Voiture
#define LedModeJN 13       // Led pour choix : Mode Jour/Nuit

#define OutCA1 8  // Sortie pour piloter contacteur Chauffe Eau 1
#define OutCA2 9  // Sortie pour piloter contacteur Chauffe Eau 2
#define OutV 10   // Sortie pour piloter contacteur Voiture

// Modes de fonctionnement
#define ModeJN 0              // Mode Jour-Nuit
#define ModeJNR 1             // Mode Jour-Nuit Rotatif
#define ModeSolCA 2           // Mode Soleil Chauffe Eau -- Nuit : Voiture
#define ModeSolCAVR 3         // Mode Soleil Chauffe Eau Voiture Rotatif
#define ModeSolAuto 4         // Mode Auto
#define ModeSolAutoR 5        // Mode Auto avec retour en arrière vers mode Auto
#define ModeDynChangeTempo 6  // Mode Changement tempo dynamique

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

// Durée chargement de voiture en Mode forcé
//   Valeur multiplicateur en (s)
//   --> 10 heures = (3600 * 10)
#define ArmVDuration_Real 1000ul * (3600 * 10)
#define ArmVDuration_Simul 1000ul * (20)

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

// Définit la durée d'interval représentant 1 Heure
// Défaut = 1000ul * (60 * 60) = 3600000ul
#define PeriodeHeureSoleilInterval_Real 1000ul * (60 * 60)
#define PeriodeHeureSoleilInterval_Simul 1000ul * 5

// Définit la durée de Période jour en mode auto
// Nous faisons un test au bout de 7 jours pour voir si nous avons eu
// assez de Soleil sur cette période --> Par Défaut 7 jours
#define NbPeriodeJourModeAuto_Real 7
#define NbPeriodeJourModeAuto_Simul 3

// Nombre d'heure de soleil
// Définit la durée minimale d'heure de soleil sur
// la période NbPeriodeJourModeAuto avant de basculer en mode JNR
// Pour NbPeriodeJourModeAuto : 7 Jours = 56 Heures de soleil Max,
// il faudrait au moins 24 Heures de Soleil = 6 heures pour chauffer les CA
#define QuotaMiniHeureSoleil_Real 24
#define QuotaMiniHeureSoleil_Simul 4

// ======================================= VARIABLES  ===============================

// Variable Buttons
boolean ButModeJNwasUp;
boolean ButModeSolCAVwasUp;
boolean ButModeSolAutowasUp;
boolean ButArmwasUp;

// Variable Bouton : Vérifie si un bouton est pressé 2 fois
boolean ButSecondPush;

// Variable Bouton : Vérifie si un bouton Arm est pressé 2 fois
boolean ButArmSecondPush;

// Variable Bouton : Appui simultané des 3 boutons
boolean ButModeDynChangeOneShot;

// Temps
unsigned long CurrentMillis;

// Tempos
boolean NormalTempoInterval;
unsigned long val_ArmDuration;
unsigned long val_SwitchContactInterval;
unsigned long val_PeriodeJourInterval;
unsigned long val_PeriodeHeureSoleilInterval;
unsigned long val_NbPeriodeJourModeAuto;
unsigned long val_QuotaMiniHeureSoleil;

// Variable "temps" pour contôler clignottement de toutes les LEDs
unsigned long LedPreviousMillis;
unsigned long LedArmPreviousMillis;

// Variable clignottement Led
int LedInterval = LedIntervalSlow;
int LedArmInterval = LedIntervalSlow;

// Variable temps pour contrôler les contacteurs
unsigned long SwitchContactPreviousMillis;

// Variable temps pour contrôler l'armement du bouton Charge Forcer Voiture
unsigned long ArmVPreviousMillis;

// Variable de rotation pour balayer les contacteurs
//    0=CA1 , 1=CA2 , 2=Voiture
int SwitchContactSelection;

// Etat Clignottement des LEDs
int LedBlinkingState;  // LOW ou HIGH, Valeur de Clignottement de la Led

// Etat LED pour armement de la voiture
int LedArmBlinkingState;  // LOW ou HIGH, Valeur de Clignottement de la Led

// Etat de L'armement.
boolean ArmTriggerStatus;
boolean ArmDoubleTriggerStatus;

// Variable Mode de fonctionnement
int Mode;
int ModeArm;
int ModeSaved;

// Compte le Nombre de Période Jour
unsigned long PeriodeJourPreviousMillis;
unsigned long NbPeriodeJour;

// Variable pour compter le Quota de Période de Soleil
unsigned long PeriodeHeureSoleilPreviousMillis;
unsigned long QuotaHeureSoleil;

// En relation avec le mode Auto
//    Dépassement Quota, pour indiquer que l'on ne respecte pas la durée de soleil
boolean DepassementQuota;


// ======================================= SETUP ===============================
void setup() {

  if (Debug_Mode_Serie)
    Serial.begin(9600);

  pinMode(InCurrentSol, INPUT);
  pinMode(InCurrentJN, INPUT);

  pinMode(ButModeJN, INPUT_PULLUP);
  pinMode(ButModeSolAuto, INPUT_PULLUP);
  pinMode(ButModeSolCAV, INPUT_PULLUP);
  pinMode(ButArm, INPUT_PULLUP);

  pinMode(LedModeSolCAV, OUTPUT);
  pinMode(LedModeSolAuto, OUTPUT);
  pinMode(LedModeJN, OUTPUT);
  pinMode(LedArm, OUTPUT);

  pinMode(OutCA1, OUTPUT);
  pinMode(OutCA2, OUTPUT);
  pinMode(OutV, OUTPUT);

  digitalWrite(LedModeJN, HIGH);
  digitalWrite(LedModeSolCAV, LOW);
  digitalWrite(LedModeSolAuto, LOW);
  digitalWrite(LedArm, LOW);
  DeActiveRelay(OutCA1);
  DeActiveRelay(OutCA2);
  DeActiveRelay(OutV);

  // Temps
  CurrentMillis = 0;

  // Tempos
  if (BoardType == BoardTinkercad) {
    val_ArmDuration = ArmVDuration_Simul;
    val_SwitchContactInterval = SwitchContactInterval_Simul;
    val_PeriodeJourInterval = PeriodeJourInterval_Simul;
    val_PeriodeHeureSoleilInterval = PeriodeHeureSoleilInterval_Simul;
    val_NbPeriodeJourModeAuto = NbPeriodeJourModeAuto_Simul;
    val_QuotaMiniHeureSoleil = QuotaMiniHeureSoleil_Simul;
    NormalTempoInterval = false;
  } else {
    val_ArmDuration = ArmVDuration_Real;
    val_SwitchContactInterval = SwitchContactInterval_Real;
    val_PeriodeJourInterval = PeriodeJourInterval_Real;
    val_PeriodeHeureSoleilInterval = PeriodeHeureSoleilInterval_Real;
    val_NbPeriodeJourModeAuto = NbPeriodeJourModeAuto_Real;
    val_QuotaMiniHeureSoleil = QuotaMiniHeureSoleil_Real;
    NormalTempoInterval = true;
  }

  // En relation avec le mode Auto
  DepassementQuota = 0;

  // Variable pour contrôler les contacteurs
  SwitchContactPreviousMillis = CurrentMillis;
  SwitchContactSelection = 0;

  // Variable pour Armement
  LedPreviousMillis = CurrentMillis;
  LedArmPreviousMillis = CurrentMillis;
  LedArmBlinkingState = LOW;

  // Varaible pour led clignottement en fonctionnement Forcé
  LedBlinkingState = LOW;

  // Tempo d'armement
  ArmVPreviousMillis = CurrentMillis;

  // Variable Mode de fonctionnement
  Mode = ModeJNR;
  ModeArm = ModeNoARM;
  ModeSaved = ModeJNR;

  // Variable Button
  ButModeJNwasUp = true;
  ButModeSolAutowasUp = true;
  ButModeSolCAVwasUp = true;
  ButArmwasUp = true;

  ButSecondPush = false;
  ButArmSecondPush = false;

  ButModeDynChangeOneShot = true;

  // Variable Mode Auto
  QuotaHeureSoleil = 0;
  PeriodeHeureSoleilPreviousMillis = CurrentMillis;
  NbPeriodeJour = 0;
  PeriodeJourPreviousMillis = CurrentMillis;
}

// ======================== LOOP ====================================
void loop() {

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
    if (Mode == ModeJN) {
      if (SwitchContactSelection > 1) SwitchContactSelection = 0;
    } else if (Mode == ModeSolCA) {
      if (SwitchContactSelection > 1) SwitchContactSelection = 0;
    } else {
      if (SwitchContactSelection > 2) SwitchContactSelection = 0;
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
  boolean ButModeSolCAVisUp = digitalRead(ButModeSolCAV);
  boolean ButModeSolAutoisUp = digitalRead(ButModeSolAuto);
  boolean ButArmisUp = digitalRead(ButArm);

  // --------------------------------------------------
  // Cas particulier ou 3 boutons appuiés en même temps
  // --------------------------------------------------

  //
  if (ButModeDynChangeOneShot && !ButModeJNisUp && !ButModeSolCAVisUp && !ButModeSolAutoisUp) {
    delay(10);
    ButModeJNisUp = digitalRead(ButModeJN);
    ButModeSolCAVisUp = digitalRead(ButModeSolCAV);
    ButModeSolAutoisUp = digitalRead(ButModeSolAuto);

    if (!ButModeJNisUp && !ButModeSolCAVisUp && !ButModeSolAutoisUp) {
      Mode = ModeDynChangeTempo;
    }
  }

  // --------------------------
  // si bouton Mode JN pressé
  // --------------------------
  if (ButModeJNwasUp && !ButModeJNisUp) {
    delay(10);
    ButModeJNisUp = digitalRead(ButModeJN);
    if (!ButModeJNisUp) {

      if ((Mode != ModeJNR) && (Mode != ModeJN))
        ButSecondPush = false;

      if (ButSecondPush == false) {
        Mode = ModeJN;
        ButSecondPush = true;
      } else {
        Mode = ModeJNR;
        ButSecondPush = false;
      }
    }

    // Init Variables
    // On remet l'armement à zéro
    ModeArm = ModeNoARM;

    ArmTriggerStatus = false;
    ArmDoubleTriggerStatus = false;
    DepassementQuota = 0;
    SwitchContactSelection = 0;
    SwitchContactPreviousMillis = CurrentMillis;
    LedPreviousMillis = CurrentMillis;
    ArmVPreviousMillis = CurrentMillis;
    QuotaHeureSoleil = 0;
    PeriodeHeureSoleilPreviousMillis = CurrentMillis;
    NbPeriodeJour = 0;
    PeriodeJourPreviousMillis = CurrentMillis;
    ButModeDynChangeOneShot = true;
    ButArmSecondPush = false;

    DeActiveRelay(OutCA1);
    DeActiveRelay(OutCA2);
    DeActiveRelay(OutV);
    delay(10);
  }
  ButModeJNwasUp = ButModeJNisUp;  // true = bouton relaché  --> mémorise l'état


  // --------------------------
  // si bouton Mode Soleil CAV pressé
  // --------------------------
  if (ButModeSolCAVwasUp && !ButModeSolCAVisUp) {
    delay(10);
    ButModeSolCAVisUp = digitalRead(ButModeSolCAV);
    if (!ButModeSolCAVisUp) {

      if ((Mode != ModeSolCAVR) && (Mode != ModeSolCA))
        ButSecondPush = false;

      if (ButSecondPush == false) {
        Mode = ModeSolCA;
        ButSecondPush = true;
      } else {
        Mode = ModeSolCAVR;
        ButSecondPush = false;
      }

      // Init Variables
      // On remet l'armement à zéro
      ModeArm = ModeNoARM;

      ArmTriggerStatus = false;
      ArmDoubleTriggerStatus = false;
      DepassementQuota = 0;
      SwitchContactSelection = 0;
      SwitchContactPreviousMillis = CurrentMillis;
      LedPreviousMillis = CurrentMillis;
      ArmVPreviousMillis = CurrentMillis;
      QuotaHeureSoleil = 0;
      PeriodeHeureSoleilPreviousMillis = CurrentMillis;
      NbPeriodeJour = 0;
      PeriodeJourPreviousMillis = CurrentMillis;
      ButModeDynChangeOneShot = true;

      DeActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      DeActiveRelay(OutV);
      delay(10);
    }
  }
  ButModeSolCAVwasUp = ButModeSolCAVisUp;  // = true bouton relaché  --> mémorise l'état

  // --------------------------
  // si bouton Mode Auto pressé
  // --------------------------
  if (ButModeSolAutowasUp && !ButModeSolAutoisUp) {
    delay(10);
    ButModeSolAutoisUp = digitalRead(ButModeSolAuto);
    if (!ButModeSolAutoisUp) {

      if ((Mode != ModeSolAutoR) && (Mode != ModeSolAuto))
        ButSecondPush = false;

      if (ButSecondPush == false) {
        Mode = ModeSolAuto;
        ButSecondPush = true;
      } else {
        Mode = ModeSolAutoR;
        ButSecondPush = false;
      }

      // Init Variables
      // On remet l'armement à zéro
      ModeArm = ModeNoARM;

      ArmTriggerStatus = false;
      ArmDoubleTriggerStatus = false;
      DepassementQuota = 0;
      SwitchContactSelection = 0;
      SwitchContactPreviousMillis = CurrentMillis;
      LedPreviousMillis = CurrentMillis;
      ArmVPreviousMillis = CurrentMillis;
      QuotaHeureSoleil = 0;
      PeriodeHeureSoleilPreviousMillis = CurrentMillis;
      NbPeriodeJour = 0;
      PeriodeJourPreviousMillis = CurrentMillis;
      ButModeDynChangeOneShot = true;
      ButArmSecondPush = false;

      DeActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      DeActiveRelay(OutV);
      delay(10);
    }
  }
  ButModeSolAutowasUp = ButModeSolAutoisUp;  // = true bouton relaché  --> mémorise l'état

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

    case ModeJNR:
      WorkMode_JNR(LOW);
      break;

    case ModeSolCA:
      WorkMode_SolCA();
      break;

    case ModeSolCAVR:
      WorkMode_SolCAVR(LOW);
      break;

    case ModeSolAuto:
      WorkMode_Auto();
      break;

    case ModeSolAutoR:
      WorkMode_AutoR();
      break;

    case ModeDynChangeTempo:
      WorkMode_DynChangeTempo();
      break;

    default:
      WorkMode_JN();
      break;
  }

  if (Debug_Mode_Serie) {
    Serial.print("Md=");
    Serial.print(Mode);
    Serial.write("   ");

    Serial.print("Ma=");
    Serial.print(ModeArm);
    Serial.write("   ");

    Serial.print("H.Sol/(H=5s)<4> =");
    Serial.print(QuotaHeureSoleil);
    Serial.write("   ");

    Serial.print("N.PJrs/(J=15s)[3] =");
    Serial.print(NbPeriodeJour);
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
void WorkMode_DynChangeTempo() {
  int i;

  for (i = 0; i < 5; i++) {
    digitalWrite(LedModeJN, HIGH);
    digitalWrite(LedModeSolCAV, HIGH);
    digitalWrite(LedModeSolAuto, HIGH);
    delay(500);
    digitalWrite(LedModeJN, LOW);
    digitalWrite(LedModeSolCAV, LOW);
    digitalWrite(LedModeSolAuto, LOW);
    delay(500);
  }

  if (NormalTempoInterval) {
    val_ArmDuration = ArmVDuration_Simul;
    val_SwitchContactInterval = SwitchContactInterval_Simul;
    val_PeriodeJourInterval = PeriodeJourInterval_Simul;
    val_PeriodeHeureSoleilInterval = PeriodeHeureSoleilInterval_Simul;
    val_NbPeriodeJourModeAuto = NbPeriodeJourModeAuto_Simul;
    val_QuotaMiniHeureSoleil = QuotaMiniHeureSoleil_Simul;
    NormalTempoInterval = false;
  } else {
    val_ArmDuration = ArmVDuration_Real;
    val_SwitchContactInterval = SwitchContactInterval_Real;
    val_PeriodeJourInterval = PeriodeJourInterval_Real;
    val_PeriodeHeureSoleilInterval = PeriodeHeureSoleilInterval_Real;
    val_NbPeriodeJourModeAuto = NbPeriodeJourModeAuto_Real;
    val_QuotaMiniHeureSoleil = QuotaMiniHeureSoleil_Real;
    NormalTempoInterval = true;
  }
  ButModeDynChangeOneShot = false;
  Mode = ModeJN;
}


// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
/* 
      -------------------------------------------
      * Mode JN : Jour-Nuit --> Led : Bleue Allumée
      --------------------------------------------
	 Si ModeArm = 0
        Si signal J/N = 1
          Basculement entre pilotage "CA1 + CA2" puis "CV"
        Si signal J/N = 0
          Pas de pilotage
	 Si ModeArm = 1x
        Si signal J/N = 1
          Basculement entre pilotage "CA1 + CA2 + CV" puis "CV"
        Si signal J/N = 0
          "CV"
	 Si ModeArm = 2x
          Basculement entre pilotage "CA1 + CA2" puis "CA2 + CV"
*/
void WorkMode_JN() {

  digitalWrite(LedModeJN, HIGH);
  digitalWrite(LedModeSolCAV, LOW);
  digitalWrite(LedModeSolAuto, LOW);

  if (ModeArm != ModeNoARM)
    digitalWrite(LedArm, LedArmBlinkingState);
  else
    digitalWrite(LedArm, LOW);

  // -------------------------------------
  if (ModeArm == ModeNoARM) {
    if ((digitalRead(InCurrentJN) == HIGH)) {
      if (SwitchContactSelection == 0) {
        ActiveRelay(OutCA1);
        ActiveRelay(OutCA2);
        DeActiveRelay(OutV);
      }
      if (SwitchContactSelection == 1) {
        DeActiveRelay(OutCA1);
        DeActiveRelay(OutCA2);
        ActiveRelay(OutV);
      }
    } else {
      DeActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      DeActiveRelay(OutV);
    }
  } else if (ModeArm == ModeARMSimple) {
    if ((digitalRead(InCurrentJN) == HIGH)) {
      if (SwitchContactSelection == 0) {
        ActiveRelay(OutCA1);
        ActiveRelay(OutCA2);
        ActiveRelay(OutV);
      }
      if (SwitchContactSelection == 1) {
        DeActiveRelay(OutCA1);
        DeActiveRelay(OutCA2);
        ActiveRelay(OutV);
      }
    } else {
      DeActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      ActiveRelay(OutV);
    }
  } else {
    //ModeArm == ModeARMSDouble
    if (SwitchContactSelection == 0) {
      ActiveRelay(OutCA1);
      ActiveRelay(OutCA2);
      DeActiveRelay(OutV);
    }
    if (SwitchContactSelection == 1) {
      DeActiveRelay(OutCA1);
      ActiveRelay(OutCA2);
      ActiveRelay(OutV);
    }
  }
}
// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
/*
      -------------------------------------------
      * Mode JNR : Jour-Nuit Rotatif --> Led : Bleue Clignotante
      --------------------------------------------
  	 Si ModeArm = 0
        Si signal J/N = 1
          Basculement entre pilotage "CA1" puis "CA2" puis "CV"
        Si signal J/N = 0
          Pas de pilotage
	 Si ModeArm = 1x
        Si signal J/N = 1
          Basculement entre pilotage "CA1+CV" puis "CA2+CV" puis "CV"
        Si signal J/N = 0
          "CV"
	 Si ModeArm = 2x
          Basculement entre pilotage "CA1" puis "CA2" puis "CV"

*/
void WorkMode_JNR(int ValLedModeSolAuto) {

  digitalWrite(LedModeJN, LedBlinkingState);
  digitalWrite(LedModeSolCAV, LOW);
  digitalWrite(LedModeSolAuto, LOW);

  // Clignottement ou Non Led Armement
  if (ModeArm != ModeNoARM)
    digitalWrite(LedArm, LedArmBlinkingState);
  else
    digitalWrite(LedArm, LOW);

  // -------------------------------------
  if (ModeArm == ModeNoARM) {
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
  } else if (ModeArm == ModeARMSimple) {
    if ((digitalRead(InCurrentJN) == HIGH)) {
      if (SwitchContactSelection == 0) {
        ActiveRelay(OutCA1);
        DeActiveRelay(OutCA2);
        ActiveRelay(OutV);
      }
      if (SwitchContactSelection == 1) {
        DeActiveRelay(OutCA1);
        ActiveRelay(OutCA2);
        ActiveRelay(OutV);
      }
      if (SwitchContactSelection == 2) {
        DeActiveRelay(OutCA1);
        DeActiveRelay(OutCA2);
        ActiveRelay(OutV);
      }
    } else {
      DeActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      ActiveRelay(OutV);
    }
  } else {
    //ModeArm == ModeARMSDouble
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
/*    
     -------------------------------------------
      * Mode SolCA : Soleil Chauffe Eau --> Led Orange Allumée
      --------------------------------------------
 	 Si ModeArm = 0
        Si signal SOL = 1
          Basculement entre pilotage "CA1" puis "CA2"
        Si signal J/N = 1
          Activation CV 
		Si signal J/N = 0 ou SOL = 0
          Pas de pilotage
 	 Si ModeArm = 1x
        Si signal SOL = 1
          Basculement entre pilotage "CA1+CV" puis "CA2+CV"
 	 Si ModeArm = 2x
          Basculement entre pilotage "CA1 + CV" puis "CA2 + CV"
*/

void WorkMode_SolCA() {

  digitalWrite(LedModeSolCAV, HIGH);
  digitalWrite(LedModeSolAuto, LOW);
  digitalWrite(LedModeJN, LOW);
  digitalWrite(LedArm, LOW);

  // Clignottement ou Non Led Armement
  if (ModeArm != ModeNoARM)
    digitalWrite(LedArm, LedArmBlinkingState);
  else
    digitalWrite(LedArm, LOW);
  // --------------------------------------------------
  if (ModeArm == ModeNoARM) {
    if ((digitalRead(InCurrentSol) == HIGH)) {
      if (SwitchContactSelection == 0) {
        ActiveRelay(OutCA1);
        DeActiveRelay(OutCA2);
      }
      if (SwitchContactSelection == 1) {
        DeActiveRelay(OutCA1);
        ActiveRelay(OutCA2);
      }
    } else {
      DeActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
    }
    if (digitalRead(InCurrentJN) == HIGH) {
      ActiveRelay(OutV);
    } else {
      DeActiveRelay(OutV);
    }
  } else if (ModeArm == ModeARMSimple) {
    if ((digitalRead(InCurrentSol) == HIGH)) {
      if (SwitchContactSelection == 0) {
        ActiveRelay(OutCA1);
        DeActiveRelay(OutCA2);
      }
      if (SwitchContactSelection == 1) {
        DeActiveRelay(OutCA1);
        ActiveRelay(OutCA2);
      }
    } else {
      DeActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
    }
    ActiveRelay(OutV);
  } else {
    //ModeArm == ModeARMSDouble
    if (SwitchContactSelection == 0) {
      ActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      ActiveRelay(OutV);
    }
    if (SwitchContactSelection == 1) {
      DeActiveRelay(OutCA1);
      ActiveRelay(OutCA2);
      ActiveRelay(OutV);
    }
  }
}

// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
/*
      -------------------------------------------
      * Mode SolCAVR : Soleil Rotatif --> Led Orange Clignotante
      --------------------------------------------
 	    Si ModeArm = 0
        Si signal SOL = 1
          Basculement entre pilotage "CA1" puis "CA2" puis "V"
        Si SOL = 0
          Pas de pilotage
 	    Si ModeArm = 1x
        Si signal SOL = 1
          Basculement entre pilotage "CA1+CV" puis "CA2+CV" puis "CV"
 	    Si ModeArm = 2x
          Basculement entre pilotage "CA1" puis "CA2" puis "CV"
*/

void WorkMode_SolCAVR(int ValLedModeSolAuto) {

  digitalWrite(LedModeJN, LOW);
  digitalWrite(LedModeSolCAV, LedBlinkingState);
  digitalWrite(LedModeSolAuto, ValLedModeSolAuto);
  digitalWrite(LedArm, LOW);

  // Clignottement ou Non Led Armement
  if (ModeArm != ModeNoARM)
    digitalWrite(LedArm, LedArmBlinkingState);
  else
    digitalWrite(LedArm, LOW);

  // -------------------------------------
  if (ModeArm == ModeNoARM) {
    if ((digitalRead(InCurrentSol) == HIGH)) {
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
  } else if (ModeArm == ModeARMSimple) {
    if ((digitalRead(InCurrentSol) == HIGH)) {
      if (SwitchContactSelection == 0) {
        ActiveRelay(OutCA1);
        DeActiveRelay(OutCA2);
        ActiveRelay(OutV);
      }
      if (SwitchContactSelection == 1) {
        DeActiveRelay(OutCA1);
        ActiveRelay(OutCA2);
        ActiveRelay(OutV);
      }
      if (SwitchContactSelection == 2) {
        DeActiveRelay(OutCA1);
        DeActiveRelay(OutCA2);
        ActiveRelay(OutV);
      }
    } else {
      DeActiveRelay(OutCA1);
      DeActiveRelay(OutCA2);
      ActiveRelay(OutV);
    }
  } else {
    //ModeArm == ModeARMSDouble
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

  // Nous passons dans cette procédure plusieurs fois par seconde
  // Notre hyptohèse est de considérer être en temps réel
  // Et ignorer les fluctuations qui pourraient arriver dans cet interval < 1 seconde
  digitalWrite(LedModeJN, LOW);
  digitalWrite(LedModeSolAuto, HIGH);
  digitalWrite(LedModeSolCAV, LOW);

  if (DepassementQuota) {
    WorkMode_JNR(HIGH);
  } else {
    WorkMode_SolCAVR(HIGH);

    // Le test pour rester dans le Mode SolCAV est uniquemenf fait 1 seul fois !

    // Compte Nombre de Période Jour
    if (CurrentMillis - PeriodeJourPreviousMillis >= val_PeriodeJourInterval) {
      PeriodeJourPreviousMillis = CurrentMillis;
      NbPeriodeJour = NbPeriodeJour + 1;
    }

    // Compte Nombre Période Heure de Soleil
    if (digitalRead(InCurrentSol) == HIGH) {
      if (CurrentMillis - PeriodeHeureSoleilPreviousMillis >= val_PeriodeHeureSoleilInterval) {
        PeriodeHeureSoleilPreviousMillis = CurrentMillis;
        QuotaHeureSoleil = QuotaHeureSoleil + 1;
      }
    } else {
      // Attention comme CurrentMillis évolue à chaque passage,
      // il faut biaiser en décalant l'origine dans le cas où il n'y
      // a pas de soleil !
      PeriodeHeureSoleilPreviousMillis = CurrentMillis;

      /*
				CurrentMillis : 25 50 75 100 125 150 175
				PeriodeHeureSoleilInterval = 50

				Sol = 1

				PeriodeHeureSoleilPreviousMillis = 0
				CurrentMillis 
				25 50
				CurrentMillis - PeriodeHeureSoleilPreviousMillis
				25 50

				PeriodeHeureSoleilPreviousMillis = 50

				CurrentMillis	
				75 100
				CurrentMillis - PeriodeHeureSoleilPreviousMillis
				25 50 
				PeriodeHeureSoleilPreviousMillis = 100

				Sol = 1
				CurrentMillis	
				125
				CurrentMillis - PeriodeHeureSoleilPreviousMillis
				25 

				Sol = 0
				CurrentMillis	
				150	175 200
					--> PeriodeHeureSoleilPreviousMillis = 200
									
				Sol = 1
				CurrentMillis	
				225
				CurrentMillis - PeriodeHeureSoleilPreviousMillis
				25		*/
    }

    // Nous sommes arrivé à la fin de la période de comptage
    // Nous vérifions si le quota du nombre d'heure est atteint
    if (NbPeriodeJour >= val_NbPeriodeJourModeAuto) {
      if (QuotaHeureSoleil >= val_QuotaMiniHeureSoleil) {
        // Le quota est respecté, nous restons dans le même mode
        // ON redémarre compteur
        QuotaHeureSoleil = 0;
        NbPeriodeJour = 0;
        PeriodeHeureSoleilPreviousMillis = CurrentMillis;
        PeriodeJourPreviousMillis = CurrentMillis;

        DepassementQuota = false;
      } else {
        // Le quota n'est pas respecté, nous passons dans le même mode JNR et nous y restons
        QuotaHeureSoleil = 0;
        NbPeriodeJour = 0;
        PeriodeHeureSoleilPreviousMillis = CurrentMillis;
        PeriodeJourPreviousMillis = CurrentMillis;

        DepassementQuota = true;
      }
    }
  }
}

// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------

void WorkMode_AutoR() {

  digitalWrite(LedModeJN, LOW);
  digitalWrite(LedModeSolAuto, LedBlinkingState);
  digitalWrite(LedModeSolCAV, LOW);

  if (DepassementQuota) {
    WorkMode_JNR(LedBlinkingState);
  } else {
    WorkMode_SolCAVR(LedBlinkingState);
  }

  // Le test pour revenir vers le Mode SolCAV est fait à chaque fois !
  if (CurrentMillis - PeriodeJourPreviousMillis >= val_PeriodeJourInterval) {
    PeriodeJourPreviousMillis = CurrentMillis;
    NbPeriodeJour = NbPeriodeJour + 1;
  }

  // Compte Nombre Période Heure de Soleil
  if (digitalRead(InCurrentSol) == HIGH) {
    if (CurrentMillis - PeriodeHeureSoleilPreviousMillis >= val_PeriodeHeureSoleilInterval) {
      PeriodeHeureSoleilPreviousMillis = CurrentMillis;
      QuotaHeureSoleil = QuotaHeureSoleil + 1;
    }
  } else {
    // Attention comme CurrentMillis évolue à chaque passage,
    // il faut biaiser en décalant l'origine dans le cas où il n'y
    // a pas de soleil !
    PeriodeHeureSoleilPreviousMillis = CurrentMillis;
  }

  // Nous sommes arrivé à la fin de la période de comptage
  // Nous vérifions si le quota du nombre d'heure est atteint
  if (NbPeriodeJour >= val_NbPeriodeJourModeAuto) {
    if (QuotaHeureSoleil >= val_QuotaMiniHeureSoleil) {
      // Le quota est respecté, nous restons dans le même mode
      // ON redémarre compteur
      QuotaHeureSoleil = 0;
      NbPeriodeJour = 0;
      PeriodeHeureSoleilPreviousMillis = CurrentMillis;
      PeriodeJourPreviousMillis = CurrentMillis;

      DepassementQuota = false;
    } else {
      // Le quota n'est pas respecté, nous passons dans le même mode JNR
      QuotaHeureSoleil = 0;
      NbPeriodeJour = 0;
      PeriodeHeureSoleilPreviousMillis = CurrentMillis;
      PeriodeJourPreviousMillis = CurrentMillis;

      DepassementQuota = true;
    }
  }
}

// -------------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------------
