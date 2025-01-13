/* =================================================================
	Pilote Arduino pour optimiser la gestion de consommation électrique
    Christian Klugesherz
    Date : 12 janvier 2025 --> Nouvelle approche avec double fonction de boutons
	
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
	  * BoutonJN --> Permet aussi de forcer CA1
	  * BoutonSOL--> Permet aussi de forcer CA2
	  * BoutonAUTO--> Permet aussi de forcer V
    Modes :
	  Dans le principe, un changement de mode, va re-initialiser le compteur d'armement

Les Modes disponibles :

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
 
      -------------------------------------------
      * Bouton Armement pressé 2X --> Led Blanche Clignotante Rapide
      --------------------------------------------
			Quelque soit le mode : ModeArm = 2
				Sans courant de nuit, ni Soleil  : Basculement entre pilotage "CA1" puis "CA2" puis "V" sur une durée 
					définie : ArmDuration

      -------------------------------------------
      * Bouton Forcage JN=CA1 ou SOL=CA2 ou AUTO=V 
      -------------------------------------------
      Un premier appui sur le bouton va 
        * Positionner le forcage, 
      un deuxième va 
        * Positionner le mode 
        