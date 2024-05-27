# apoce
Pilote Arduino pour optimiser la gestion de consommation électrique

    Christian Klugesherz
    Date : 25 janvier 2024
    La simulation se trouve sur
    https://www.tinkercad.com/things/i7El4JjrINq-pilote-contacteur

	ATTENTION:
	===========
		Il est impératif de configurer la partie Configuration ci-dessous !
		
    Principe
    =========
    Entrée :
      * Signal Jour/Nuit - J/N
      * Signal Soleil - SOL
    Sortie :
      * Signal pour piloter contacteur Chauffe Eau 1 - CA1
      * Signal pour piloter contacteur Chauffe Eau 2 - CA2
      * Signal pour piloter contacteur Chargement Voiture - CV
    Modes :
		Dans le principe, un changement de mode, va éinitialiser le compteur d'armement
		
	Les Modes disponibles :
      -------------------------------------------
      * Mode JN : Jour-Nuit --> Led : Bleue Allumée
      --------------------------------------------
        Si signal J/N = 1
          Basculement entre pilotage "CA1 + CA2" puis "CV"
        Si signal J/N = 0
          Pas de pilotage

      -------------------------------------------
      * Mode JNR : Jour-Nuit Rotatif --> Led : Bleue Clignotante
      --------------------------------------------
        Si signal J/N = 1
          Basculement entre pilotage "CA1" puis "CA2" puis "CV"
        Si signal J/N = 0
          Pas de pilotage

      -------------------------------------------
      * Mode SolCA : Soleil Chauffe Eau --> Led Orange Allumée
      --------------------------------------------
        Si signal SOL = 1
          Basculement entre pilotage "CA1" puis "CA2"
        Si signal J/N = 1
          Activation CV 
        Si signal J/N = 0 ou SOL = 0
          Pas de pilotage

      -------------------------------------------
      * Mode SolCAVR : Soleil Rotatif --> Led Orange Clignotante
      --------------------------------------------
        Si signal SOL = 1
          Basculement entre pilotage "CA1" puis "CA2" puis "V"
        Si SOL = 0
          Pas de pilotage

      -------------------------------------------
      * Mode Auto : Mode SolCAVR avec repliement vers JNR 
		  --> Led Orange Clignotante
		  --> Led Rouge : Fixe
      --------------------------------------------
		    Même chose que Mode SolCAVR
		    Si après NbPeriodeJour, il a y eu moins de Soleil que "QuotaMiniHeureSoleil"
		    Alors, Nous nous replions vers le Mode JNR et on y reste

      -------------------------------------------
      * Mode AutoR : Mode SolCAVR avec repliement vers JNR et retour vers SolCAVR possible 
		  --> Led Orange Clignotante
		  --> Led Rouge : Clignatante
      --------------------------------------------
		    Même chose que Mode SolCAVR
		    Si après NbPeriodeJour, il a y eu moins de Soleil que "QuotaMiniHeureSoleil"
		    Alors, Nous nous replions vers le Mode JNR 
		    Si après NbPeriodeJour, il a y plus de Soleil que "QuotaMiniHeureSoleil"
		    Alors, Nous revenns vers le Mode SolCAVR 

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
					
					