#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "GrilleSDL.h"
#include "Ressources.h"


// --- Dimensions de la grille de jeu --- //
#define NB_LIGNES   12
#define NB_COLONNES 19

// --- Nombre de cases maximum par piece --- //
#define NB_CASES    4

// --- Macros utilisees dans le tableau tab --- //
#define VIDE        0
#define BRIQUE      1
#define DIAMANT     2

int tab[NB_LIGNES][NB_COLONNES]
={ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};

typedef struct
{
	int ligne;
	int colonne;
} CASE;

typedef struct
{
	CASE cases[NB_CASES];
	int  nbCases;
	int  couleur;
} PIECE;

PIECE pieces[12] = { 0,0,0,1,1,0,1,1,4,0,       // carre 4
                     0,0,1,0,2,0,2,1,4,0,       // L 4
                     0,1,1,1,2,0,2,1,4,0,       // J 4
                     0,0,0,1,1,1,1,2,4,0,       // Z 4
                     0,1,0,2,1,0,1,1,4,0,       // S 4
                     0,0,0,1,0,2,1,1,4,0,       // T 4
                     0,0,0,1,0,2,0,3,4,0,       // I 4
                     0,0,0,1,0,2,0,0,3,0,       // I 3
                     0,1,1,0,1,1,0,0,3,0,       // J 3
                     0,0,1,0,1,1,0,0,3,0,       // L 3
                     0,0,0,1,0,0,0,0,2,0,       // I 2
                     0,0,0,0,0,0,0,0,1,0 };     // carre 1


#define handle_error(msg) \
          do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define TAILLE_BANNER 17


// === VARIABLES GLOBALES POUR LES EVENEMENTS

EVENT_GRILLE_SDL event;

/* === DECLARATION DES VARIABLES DE TRAVAIL POUR LA FILE DE MESSAGE == */

char *message;      //  Pointeur vers le message à faire défiler
int tailleMessage;  //  Longueur du message 
int indiceCourant;  //  Indice du premier caractère à afficher dans la zone graphique

// === VARIABLE GLOBALES POUR LA PIECE EN COURS ===

PIECE pieceEnCours;

// === VARIABLES GLOBALES POUR LES CASES INSEREES ===

CASE casesInserees[NB_CASES]; 	//  Cases insérées par le joueur 
int nbCasesInserees = 0; 		//  Nombre de cases actuellement insérées par le joueur


// == VARIABLES GLOBALES POUR LES SCORES ET COMBOS ==

bool MAJScore 	= true;
int score 		= 0;
bool MAJCombos 	= true;
int combos 		= 0;


// === VARIABLES GLOBALES POUR LES ANALYSES ===

int lignesCompletes[NB_CASES];
int nbLignesCompletes;
int colonnesCompletes[NB_CASES];
int nbColonnesCompletes;
int carresComplets[NB_CASES];
int nbCarresComplets;
int nbAnalyses;


// === VARIABLES GLOBALES THREADCASES,THREADNETTOYEUR ===

// Vecteur qui contient les valeur minimales et maximales des lignes et des colonnes 
int MinSquares[3] = {0,3,6}, MaxSquares[3] = {2,5,8}; 


// === VARIABLE GLOBALE POUR LE TRAITEMENT EN COURS ===

bool TraitementEnCours = false;

// === VARIABLES GLOBALES POUR LES PRIMITIVES ===

pthread_key_t cle;

// ===VARIABLES GLOBALES POUR LES TIDS ===

pthread_t threadHandleFileMessage;
pthread_t threadHandleEvent;
pthread_t threadHandlePiece;
pthread_t threadHandleNettoyeur;
pthread_t threadHandleScore;
pthread_t tabThreadCase[9][9];	// Tableau qui va contenir l'ensemble des identifiants de threadCases (81) --- //

// === MUTEX POUR LA FILE DE MESSAGE ===

pthread_mutex_t mutexMessage = PTHREAD_MUTEX_INITIALIZER;


// === MUTEX GLOBALES POUR LES CASES INSEREES ===

pthread_mutex_t mutexCasesInserees = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condCasesInserees;

// === MUTEX POUR LES SCORES ===

pthread_mutex_t mutexScore = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condScore;


// === MUTEX POUR LES ANALYSES ===

pthread_mutex_t mutexAnalyse = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condAnalyse;


// === MUTEX GLOBALES POUR LE TRAITEMENT EN COURS ===

pthread_mutex_t mutexTraitement = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condTraitement;


// === Les prototypes pour les threads ===

void * threadDefileMessage(void);
void * threadPiece(void);
void * threadEvent(void);
void * threadScore(void);
void * threadCases(CASE *);
void * threadNettoyeur(void);
void * threadFin(void);

// === Les prototypes des fonctions ===

int  	CompareCases(CASE case1,CASE case2);
void 	TriCases(CASE *vecteur,int indiceDebut,int indiceFin);



void 	setMessage(const char *, bool);

void 	RotationPiece(PIECE *);
int 	RandomCouleurPiece(void);
void 	CreerPiece(void);


int 	RechercheCarre(CASE *);
void 	TerminaisonCle(void*data);


bool isColonneNotEmpty(CASE *pCase);
bool isLigneNotEmpty(CASE *pCase);


void 	SuppressionColonneFusion(void);
void 	SuppressionLigneFusion(void);
void 	SuppressionCarreFusion(void);


// === Les Signaux ===
void HandlerSIGALARM(int);
void HandlerSIGUSR1(int);


struct timespec mainTimer;

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{

	// Déclaration des variables de travail
	bool finProgramme = false;
	int l = 0, c = 0, i;

	CASE *pCase; // Pointeur d'une structure de case qui correspond à la case pour chaque threadCases
	
	

	srand((unsigned) time(NULL));
	
	// --- Ouverture de la fenetre graphique --- //
	printf("(MAIN %d) Ouverture de la fenetre graphique\n",pthread_self()); fflush(stdout);
	if (OuvertureFenetreGraphique() < 0)
	{
		printf("Erreur de OuvrirGrilleSDL\n");
		fflush(stdout);
		exit(1);
	}


	sigset_t mask;
  	sigfillset(&mask); // On masque l'ensemble des signaux
  	pthread_sigmask(SIG_SETMASK, &mask, NULL);	



	pthread_mutex_init(&mutexMessage, NULL);
	pthread_mutex_init(&mutexCasesInserees, NULL);
	pthread_cond_init(&condCasesInserees, NULL);

	pthread_mutex_init(&mutexScore, NULL);
	pthread_cond_init(&condScore, NULL);

	pthread_mutex_init(&mutexTraitement, NULL);
	pthread_cond_init(&condTraitement, NULL);

	// Alloue une nouvelle clé TSD ((Thread-Specific Data), qui est enregistrée à l'emplacement pointée par clé
	pthread_key_create(&cle, (void (*)(void *))TerminaisonCle ); 

	setMessage("Bienvenue sur Blockudoku", true);


	pthread_create(&threadHandleFileMessage, NULL, (void *(*)(void *))threadDefileMessage, NULL);
	pthread_create(&threadHandlePiece, NULL, (void *(*)(void *))threadPiece, NULL);
	pthread_create(&threadHandleEvent, NULL, (void *(*)(void *))threadEvent, NULL);
	pthread_create(&threadHandleScore, NULL, (void *(*)(void *))threadScore, NULL);

	// === Création des threadCases (81) ===
	for(l = 0; l < 9; l++)
  	{
	    for(c = 0; c < 9; c++)
	    {
	      pCase = (CASE *) malloc(sizeof(CASE));
	      pCase->ligne = l;
	      pCase->colonne = c;
	      pthread_create(&tabThreadCase[l][c], NULL, (void *(*)(void *))threadCases, pCase);
	    }
  	}

	// Création du threadNettoyeur 
	pthread_create(&threadHandleNettoyeur, NULL, (void *(*)(void *))threadNettoyeur, NULL);



	pthread_mutex_lock(&mutexAnalyse); // LOCK
	// --- Initialisation d'une valeur par défaut pour chaque vecteur pour être sûr de ne pas avoir des valeurs aléatoires --- //
	for(i = 0; i < NB_CASES; i++)
	{
		lignesCompletes[i] = -1;
		colonnesCompletes[i] = -1;
		carresComplets[i] = -1;
	}

	nbLignesCompletes = 0;
	nbColonnesCompletes = 0;
	nbCarresComplets = 0;
	nbAnalyses = 0;

	pthread_mutex_unlock(&mutexAnalyse); // UNLOCK

	DessineVoyant(8, 10, VERT);

	// Attendre que le threadPiece se termine 
	pthread_join(threadHandlePiece, NULL);

	pthread_cancel(threadHandleEvent);

	for(l = 0; l < 9; l++)
	{
		for(c = 0; c < 9; c++)
		{
			pthread_cancel(tabThreadCase[l][c]);
		}
	}
	
	while(!finProgramme)
	{
		event = ReadEvent();

		if(event.type == CROIX) 
		{
			finProgramme = true;
		}
	}

	pthread_cancel(threadHandleFileMessage);

	// Fermeture de la fenetre
	printf("(MAIN %d) Fermeture de la fenetre graphique...", pthread_self()); fflush(stdout);
	FermetureFenetreGraphique();
	printf("\nOK\n");

	exit(0);
}


/*
 * @brief Fonction qui s'occupe de la file de message
 * @return Pointeur générique (void*)
 */
void* threadDefileMessage(void)
{

  // L'empilement d'un appel à une fonction de terminaison
  pthread_cleanup_push((void (*) (void *))threadFin, 0);


  // === DECLARATION DES VARIABLES DE TRAVAIL ===
  int positionBuffer;

  struct sigaction sigact;
  
  sigact.sa_handler = HandlerSIGALARM;
  sigact.sa_flags = 0;
  
  
  sigfillset(&sigact.sa_mask); // On masque l'ensemble des signaux
  sigdelset(&sigact.sa_mask, SIGALRM); // On retire du masquage le signal SIGALRM
  
  pthread_sigmask(SIG_SETMASK, &sigact.sa_mask, NULL);
  if ((sigaction(SIGALRM, &sigact, 0)) == -1)
    handle_error( "Erreur : sigaction" );
  
  if(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL))
	fprintf(stderr, "ERREUR SETCANCELSTATE : threadDefileMessage\n");

	if(pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL))
	fprintf(stderr, "ERREUR SETCANCELSTATE : threadDefileMessage\n");
  
  
  
  mainTimer.tv_sec = 0;
  mainTimer.tv_nsec = 400000000; // 0.4 secondes
  
  positionBuffer = 0;

  // === TRAITEMENT ===
  while(1)
  {

  	pthread_mutex_lock(&mutexMessage);

    for(positionBuffer = 0; positionBuffer < TAILLE_BANNER; ++positionBuffer)
    {
  	  DessineLettre(10, positionBuffer + 1, *(message + ((indiceCourant + positionBuffer) % tailleMessage) ));
    }
    
    indiceCourant++;

    pthread_mutex_unlock(&mutexMessage);

    nanosleep(&mainTimer, NULL);
  }

  // L'appel de la fonction en fin d'execution du thread n'est pas automatique, il doit être provoqué par l'appel de la fonction dépilement
  pthread_cleanup_pop(1);
  
  pthread_exit(NULL);
}

/*
 * @brief Lorsque un thread est arrêté brutalement
 */
void * threadFin(void)
{
	fprintf(stderr, "\n\x1b[32m\x1b[1mFin du threadDefileMessage : Liberation du message\x1b[0m\n");
	free(message);
}

/*
 * @brief Fonction qui s'occupe de la génération des pièces
 * @return Pointeur générique
 */
void * threadPiece(void)
{
	// Déclaration des variables de travail
	int nbRotation = 0, i, j, LMin, CMin;
	int boolean = 1, boolean2;
	bool CaseCorrespondante;
	int NbAleatoire = 0;
	int p = 0;
	bool GameOver = false;
	int C, L;

	

	// On masque tous les signaux pour le threadPiece 
	sigset_t mask;
	sigfillset(&mask);
	pthread_sigmask(SIG_SETMASK, &mask, NULL);

	srand((unsigned) time(NULL));

	if(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL))
		handle_error("ERREUR SETCANCELSTATE : threadPiece\n");

	if(pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL))
		handle_error("ERREUR SETCANCELSTATE : threadPiece\n");


	while(boolean)
	{
		// Création de la pièce qui sera affiché dans la zone C + choix d'une couleur aléatoire de la pièce
		CreerPiece();

		// Rotation aléatoire de cette pièce allant de 0 à 3 
		NbAleatoire = rand() % 4;
		while(nbRotation < NbAleatoire)
		{
			RotationPiece(&pieceEnCours);
			nbRotation++;
		}

		// --- Tri des différentes cases de cette pièce --- //
		TriCases(pieceEnCours.cases, 0, pieceEnCours.nbCases - 1);

		// --- Positionnement de cette pièce dans la zone C pour l'affichage --- //
		for(i = 0; i < pieceEnCours.nbCases; i++)
		{
			DessineDiamant(pieceEnCours.cases[i].ligne + 3, pieceEnCours.cases[i].colonne + 14, pieceEnCours.couleur);
		}

		// On regarde s'il y a la place disponible pour insérer la pièce 
		for(i = 0; i < 9; i++) 
		{
			for(j = 0; j < 9; j++)
			{
				if(tab[i][j] == VIDE)
				{
					GameOver = false;
					// Valeurs pour la translation de la pièce en cours 
					L = i - pieceEnCours.cases[0].ligne;
					C = j - pieceEnCours.cases[0].colonne;
					for(p = 0; p < pieceEnCours.nbCases; p++)
					{
						// Vérification que nous sommes bien dans le cadre du jeu 
						if((pieceEnCours.cases[p].ligne + L < 0 || pieceEnCours.cases[p].ligne + L >= 9) || (pieceEnCours.cases[p].colonne + C < 0 || pieceEnCours.cases[p].colonne + C >= 9))
						{
							GameOver = true;
							p = pieceEnCours.nbCases;
						}
						else 
						{
							// Vérification que la case translatée est vide
							if(tab[pieceEnCours.cases[p].ligne + L][pieceEnCours.cases[p].colonne + C] != VIDE)
							{
								GameOver = true;
								p = pieceEnCours.nbCases;
							}
						}
					}
					// Une place est disponible
					if(!GameOver)
					{
						i = 9;
						j = 9;
					}
				}
			}
		}
		// On rentre dans la condition si on ne peut pas insérer la pièce en cours dans le tableau <<< //
		if(GameOver)
		{
			fprintf(stderr, "\x1b[31m\x1b[1m ====== ==== ===== [ GAME OVER ] ====== ==== =====\x1b[0m\n");
			setMessage("GAME OVER", false);
			// --- Annule le signal SIGALRM s'il y a eu un combo avant GAME OVER --- //
			alarm(0);
			pthread_exit(NULL);
		}
		


		boolean2 = 1;
		while(boolean2)
		{
			CaseCorrespondante = true;

			// Attend que l'utilisateur insère des diamants égale au nombre de case de la pièce dans la zone C
			pthread_mutex_lock(&mutexCasesInserees);
			while(nbCasesInserees < pieceEnCours.nbCases)
			{
				// Attend d'être réveillé par le threadEvent 
				pthread_cond_wait(&condCasesInserees, &mutexCasesInserees);
			}
			pthread_mutex_unlock(&mutexCasesInserees);

			//  Determiner le LMin et CMin de la pièce inséré par l'utilisateur 
			LMin = casesInserees[0].ligne;
			CMin = casesInserees[0].colonne;
			for(i = 1; i < nbCasesInserees; i++)
			{
				if(LMin > casesInserees[i].ligne)
				{
					LMin = casesInserees[i].ligne;
				}
				if(CMin > casesInserees[i].colonne)
				{
					CMin = casesInserees[i].colonne;
				}
			}

			// --- Protection des variables globales concernant les cases insérées --- //
			pthread_mutex_lock(&mutexCasesInserees);

			// --- Suppression de LMin et CMin pour chaque ligne et colonne --- //
			for(i = 0; i < nbCasesInserees; i++)
			{
				casesInserees[i].ligne -= LMin;
				casesInserees[i].colonne -= CMin;
			}

			// --- Tri des différentes cases de la pièce insérée par l'utilisateur --- //
			TriCases(casesInserees, 0, nbCasesInserees - 1);

			pthread_mutex_unlock(&mutexCasesInserees);

			// --- Vérification de la pièce insérée par l'utilisateur par rapport à la pièce en cours dans la zone C --- //
			pthread_mutex_lock(&mutexCasesInserees);
			for(i = 0; i < nbCasesInserees; i++)
			{
				if( (casesInserees[i].ligne != pieceEnCours.cases[i].ligne) || (casesInserees[i].colonne != pieceEnCours.cases[i].colonne) )
				{
					CaseCorrespondante = false;
				}
				
			}
			pthread_mutex_unlock(&mutexCasesInserees);

			// On rentre dans la condition si la pièce insérée est la même que la pièce en cours
			if(CaseCorrespondante)
			{
				// Dessine la pièce insérée par l'utilisateur en brique bleu
				pthread_mutex_lock(&mutexCasesInserees);
				for(i = 0; i < nbCasesInserees; i++)
				{
					tab[casesInserees[i].ligne + LMin][casesInserees[i].colonne + CMin] = BRIQUE;
					DessineBrique(casesInserees[i].ligne + LMin, casesInserees[i].colonne + CMin, false);
				}
				pthread_mutex_unlock(&mutexCasesInserees);

				// --- Suppression de la pièce dans la zone C --- //
				for(i = 0; i < pieceEnCours.nbCases; i++)
				{
					EffaceCarre(pieceEnCours.cases[i].ligne + 3, pieceEnCours.cases[i].colonne + 14);
				}

				// --- Protection de la variable globale score --- //
				pthread_mutex_lock(&mutexScore);

				score += pieceEnCours.nbCases;
				MAJScore = true;

				pthread_mutex_unlock(&mutexScore);

				// --- Reveille le threadScore --- //
				pthread_cond_signal(&condScore);

				// --- Protection de la variable globale concernant le traitement --- //
				pthread_mutex_lock(&mutexTraitement);

				TraitementEnCours = true;
				DessineVoyant(8, 10, BLEU);

				pthread_mutex_unlock(&mutexTraitement);

				// --- Reveille le threadCase avec le signal SIGUSR1 --- //

				pthread_mutex_lock(&mutexCasesInserees);
				for(i = 0; i < nbCasesInserees; i++)
				{
					pthread_kill(tabThreadCase[casesInserees[i].ligne + LMin][casesInserees[i].colonne + CMin], SIGUSR1);
				}
				pthread_mutex_unlock(&mutexCasesInserees);

				// --- Attend que le traitement soit effectué afin de pouvoir afficher la nouvelle pièce --- //
				// --- Attend d'être réveillé par le threadNettoyeur --- //
				pthread_mutex_lock(&mutexTraitement);
				while(TraitementEnCours)
				{
					pthread_cond_wait(&condTraitement, &mutexTraitement);
				}
				pthread_mutex_unlock(&mutexTraitement);

				boolean2 = 0;
			}
			else
			{
				// La pièce insérée n'est PAS la même que la pièce en cours 
				// Dessine la pièce insérée par l'utilisateur en brique bleu 
				pthread_mutex_lock(&mutexCasesInserees);
				for(i = 0; i < nbCasesInserees; i++)
				{
					tab[casesInserees[i].ligne + LMin][casesInserees[i].colonne + CMin] = VIDE;
					EffaceCarre(casesInserees[i].ligne + LMin, casesInserees[i].colonne + CMin);
				}
				pthread_mutex_unlock(&mutexCasesInserees);

				// Suppression des lignes et colonnes dans les casesInserees
				pthread_mutex_lock(&mutexCasesInserees);
				for(i = 0; i < nbCasesInserees; i++)
				{
					casesInserees[i].ligne = VIDE;
					casesInserees[i].colonne = VIDE;
				}
				pthread_mutex_unlock(&mutexCasesInserees);
			}

			//  Protege les variables gobales concernant les cases insérées 
			pthread_mutex_lock(&mutexCasesInserees);

			nbCasesInserees = 0;

			pthread_mutex_unlock(&mutexCasesInserees);
		}
	}
}



/**
 * @brief Fonction permettant d'envoyer un signal suivant le click de la souris.
 */
void* threadEvent(void)
{

  // Déclaration des variables de travail
  sigset_t mask;
  sigfillset(&mask); // On masque l'ensemble des signaux
  pthread_sigmask(SIG_SETMASK, &mask, NULL);
  

  struct timespec tempsEvent;

  unsigned short MainRunning = 1;
  unsigned short i = 0;
  EVENT_GRILLE_SDL event;
  

  if(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL))
	 handle_error("\x1b[31m\x1b[1mERREUR SETCANCELSTATE : threadEvent\x1b[0m\n");

  if(pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL))
	 handle_error("\x1b[31m\x1b[1mERREUR SETCANCELSTATE : threadEvent\x1b[0m\n");

  
  while(MainRunning)
  {
    event = ReadEvent();
    
    // === Ajouter un DIAMANT ===
    if(event.type == CLIC_GAUCHE)
    {
      pthread_mutex_lock(&mutexTraitement); // Protection de la variable globale concernant le traitement - LOCK

      if( (tab[event.ligne][event.colonne] == VIDE) && !TraitementEnCours && event.ligne < 9 && event.colonne < 9) 
      {
        // --- Remplissage de l'affichage + dans la matrice des cases insérées --- //
        DessineDiamant(event.ligne,event.colonne,ROUGE);
        tab[event.ligne][event.colonne] = DIAMANT;

        fprintf(stderr, "\x1b[1m\x1b[34m Ajouter = (%d, %d)\x1b[0m\n", event.ligne, event.colonne);

        // --- Protege les variables gobales concernant les cases insérées --- //
        pthread_mutex_lock(&mutexCasesInserees); // LOCK

        casesInserees[nbCasesInserees].ligne = event.ligne;
        casesInserees[nbCasesInserees].colonne = event.colonne;

        nbCasesInserees++;

        pthread_mutex_unlock(&mutexCasesInserees); // UNLOCK

        // Reveiller le threadPiece
        pthread_cond_signal(&condCasesInserees);

      } else {

      	DessineVoyant(8, 10, ROUGE);

      	fprintf(stderr, "\x1b[1m\x1b[32m[!] Dormir pour 0,40 secondes\x1b[0m\n");
		tempsEvent.tv_sec = 0;
		tempsEvent.tv_nsec = 400000000;
		nanosleep(&tempsEvent, NULL);



		if(TraitementEnCours)
			DessineVoyant(8, 10, BLEU);
		else
			DessineVoyant(8, 10, VERT);
      }

      pthread_mutex_unlock(&mutexTraitement); // UNLOCK
    }


    // === Supprimer PIECE ===
    if(event.type == CLIC_DROIT) 
    {

      // --- Protege les variables gobales concernant les cases insérées
      pthread_mutex_lock(&mutexCasesInserees);

      // --- Suppression de l'affichage + dans la matrice des cases insérées
      for(i = 0; i < nbCasesInserees; i++)
      {
        EffaceCarre(casesInserees[i].ligne, casesInserees[i].colonne);
        tab[casesInserees[i].ligne][casesInserees[i].colonne] = VIDE;
        fprintf(stderr, "\x1b[1m\x1b[31m Suppression = (%d, %d)\x1b[0m\n", casesInserees[i].ligne, casesInserees[i].colonne);
      }
      
      pthread_mutex_unlock(&mutexCasesInserees);

      // --- Protege les variables gobales concernant les cases insérées
      pthread_mutex_lock(&mutexCasesInserees);

      // --- Suppression des lignes et colonnes dans les casesInserees 
      for(i = 0; i < nbCasesInserees; i++)
      {
        casesInserees[i].ligne = VIDE;
        casesInserees[i].colonne = VIDE;
      }
  
      nbCasesInserees = 0;
      
      pthread_mutex_unlock(&mutexCasesInserees);
    }
    

    // === QUITTER ===
    if(event.type == CROIX) {
      fprintf(stderr, "\x1b[32mVous venez de quitter le jeu !\n\x1b[0m");
      MainRunning = 0;
    }
    
  }
  
  exit(0);
}


/*
 * @brief Fonction qui se charge du scores en tenant compte des combos
 */
void * threadScore(void)
{
	sigset_t mask;

	// --- On masque tous les signaux pour le threadScore pour être sûr que celui-ci ne reçoit aucun signaux --- //
	sigfillset(&mask);
	pthread_sigmask(SIG_SETMASK, &mask, NULL);

	while(1)
	{
		// === Protection des variables globale concernant le score 
		pthread_mutex_lock(&mutexScore); // LOCK

		while(!MAJScore && !MAJCombos)
			pthread_cond_wait(&condScore, &mutexScore); // Attendre d'être wake up par le threadPiece

		// === Dans le cas ou nous avons un COMBO ===
		if(MAJCombos)
		{
			DessineChiffre(8, 14, combos/1000);
			DessineChiffre(8, 15, (combos%1000)/100);
			DessineChiffre(8, 16, (combos%100)/10);
			DessineChiffre(8, 17, combos%10);

			MAJCombos = false;
		}
		
		DessineChiffre(1, 14, score/1000);
		DessineChiffre(1, 15, (score%1000)/100);
		DessineChiffre(1, 16, (score%100)/10);
		DessineChiffre(1, 17, score%10);

		MAJScore = false;
		
		pthread_mutex_unlock(&mutexScore); // UNLOCK
	}

	pthread_exit(NULL);
}

/*
 * @brief Fonction thread qui arme le signal SIGUSR1 et fait les analyses dans le HandlerSIGUSR1
 * @param CASE *pCase => un pointeur de structure de CASE mis dans une variable usuelle cle connue par tous les threads
 * @return un pointeur générique (void *)
 */
void * threadCases(CASE *pCase)
{
	// Déclaration des variables de travail 

	// Renvoie le pointeur vers la zone, ou NULL si cette zone n'existe pas encore. 
	pthread_setspecific(cle, (CASE *)pCase); // Sauvegarde la case du thread

	struct sigaction sigact;  
  	sigact.sa_handler = HandlerSIGUSR1;
  	sigact.sa_flags = 0;

  	sigfillset(&sigact.sa_mask); // On masque l'ensemble des signaux
    sigdelset(&sigact.sa_mask, SIGUSR1); // On retire du masquage le signal SIGUSR1


	pthread_sigmask(SIG_SETMASK, &sigact.sa_mask, NULL);
  	if ((sigaction(SIGUSR1, &sigact, 0)) == -1)
    	handle_error( "Erreur : sigaction" );

    // Définit l'état d'annulation du thread appelant à la valeur indiquée par <state>
    // L'ancien état d'annulation du thread est renvoyé dans le tampon pointé par <oldstate>
    // On a pas besoin de récupérer l'ancien état d'annulation du thread => NULL
	if(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL))
		fprintf(stderr, "Erreur : SETCANCELSTATE threadEvent\n\n");


	// Le thread peut être annulé à tout moment. 
	// Typiquement, il sera annulé dès réception de la requête d'annulation, mais ce n'est pas garanti par le système.
	if(pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL))
		handle_error( "Erreur : SETCANCELSTATE threadEvent\n" );

	while(1) {  // Attend d'être réveillé par le threadPiece au moyen de SIGUSR1
    	pause();
  	} 

  	// Supprimer une clé de données spécifique au thread
	if (pthread_key_delete(cle) != 0)
		handle_error( "Erreur : Lors de la Suppression de la cle de donnees" );

	pthread_exit(NULL);
}

/*
 * @brief Fonction qui se charge de désallouer la variable usuelle pour chaque threadCase

 */
void TerminaisonCle(void* data)
{
	fprintf(stderr, "\x1b[32m\x1b[1m[!] %s sur le thread %lu avec l'argument %p(%d,%d)\x1b[0m\n", __func__, pthread_self(), data, ((CASE*)data)->ligne, ((CASE*)data)->colonne);
	free(data);
}

/*
 * @brief Fonction qui permet de nettoyer les briques qui sont en fusion
 */
void * threadNettoyeur(void)
{
	// Déclaration des variables de travail
	int i = 0, j = 0;
	unsigned short nbrCombo = 0;
	struct timespec tempsNettoyeur;
	
	sigset_t mask;
  	sigfillset(&mask); // On masque l'ensemble des signaux
  	pthread_sigmask(SIG_SETMASK, &mask, NULL);

	while(1)
	{
		// --- Attend d'être réveillé par threadCases --- //
		pthread_mutex_lock(&mutexAnalyse);
		while(nbAnalyses < pieceEnCours.nbCases)
		{
			pthread_cond_wait(&condAnalyse, &mutexAnalyse);
		}
		pthread_mutex_unlock(&mutexAnalyse);

		
		// Protection des variables globales concernant l'analyse
		pthread_mutex_lock(&mutexAnalyse);

		/// On entre dans la condition s'il n'y a aucune ligne, colonne et carré complet
		if(nbLignesCompletes == 0 && nbColonnesCompletes == 0 && nbCarresComplets == 0)
		{
			nbAnalyses = 0;
		}
		else
		{
			/// On entre dans la condition s'il y a au moins une ligne, une colonne et carré complet

			// Protection des variables globales concernant le score et les combos
			pthread_mutex_lock(&mutexScore);

			// On additionne toutes les lignes, colonnes et carrés complets ce qui permet de savoir le nombre de combos effectué par l'utilisateur --- //
			nbrCombo = nbLignesCompletes + nbColonnesCompletes + nbCarresComplets;

			// Reçoit plus de points s'il y a eu plusieurs combos en même mainTimer
			switch(nbrCombo)
			{
				case 1:	
					score += 10;
					setMessage("Simple Combo", true);
					break;

				case 2:	
					score += 25;
					setMessage("Double Combo", true);
					break;

				case 3:	
					score += 40;
					setMessage("Triple Combo", true);
					break;

				case 4:	
					score += 55;
					setMessage("Quadruple Combo", true);
					break;

				default:
					break;
			}

			combos += nbrCombo;
			MAJScore = true;
			MAJCombos = true;

			pthread_mutex_unlock(&mutexScore); // UNLOCK

			// Reveille le threadScore
			pthread_cond_signal(&condScore);

			// Attendre 2 secondes :
			// Laisser du temps au joueur d'observer les lignes, colonnes, carres en fusion avant qu'ils ne disparaissent
			tempsNettoyeur.tv_sec = 2;
			tempsNettoyeur.tv_nsec = 0;
			nanosleep(&tempsNettoyeur, NULL);

			SuppressionColonneFusion();
			SuppressionLigneFusion();
			SuppressionCarreFusion();
		
			nbAnalyses = 0;
			nbLignesCompletes = 0;
			nbColonnesCompletes = 0;
			nbCarresComplets = 0;
		}

		pthread_mutex_unlock(&mutexAnalyse);

		// Protection de la variable globale concernant le traitement 
		pthread_mutex_lock(&mutexTraitement);

		TraitementEnCours = false;
		DessineVoyant(8, 10, VERT);
	
		pthread_mutex_unlock(&mutexTraitement);

		// --- Reveille threadPiece 
		pthread_cond_signal(&condTraitement);		
	}
}

/**
* @brief Fonction qui se charge de supprimer une ou plusieurs colonnes en fusion 
*/
void SuppressionColonneFusion()
{
	int i = 0, j;

	while(colonnesCompletes[i] != -1) // Check le vecteur
	{
		for(j = 0; j < 9; j++)
		{
			tab[j][colonnesCompletes[i]] = VIDE;
			EffaceCarre(j, colonnesCompletes[i]);
		}
		colonnesCompletes[i] = -1;
		i++;
	}

}



/**
* @brief Fonction qui se charge de supprimer une ou plusieurs lignes en fusion 
*/
void SuppressionLigneFusion(void)
{

	int i = 0, j;

	while(lignesCompletes[i] != -1)
	{
		for(j = 0; j < 9; j++)
		{
			tab[lignesCompletes[i]][j] = VIDE;
			EffaceCarre(lignesCompletes[i], j);
		}
		lignesCompletes[i] = -1;
		i++;
	}

}

/**
* @brief Fonction qui se charge de supprimer une ou plusieurs carres en fusion 
*/
void SuppressionCarreFusion(void)
{
	// Déclaration des variables de travail
	int i = 0;
	int ligne = 0, colonne = 0;

	while(carresComplets[i] != -1)
	{

		for(ligne = MinSquares[carresComplets[i] / 3]; ligne <= MaxSquares[carresComplets[i] / 3]; ligne++)
		{
			for(colonne = MinSquares[carresComplets[i] % 3]; colonne <= MaxSquares[carresComplets[i] % 3]; colonne++)
			{
				tab[ligne][colonne] = VIDE;
				EffaceCarre(ligne, colonne);
			}
		}
		carresComplets[i] = -1;
		i++;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * @brief Fonction qui se charge de comparer deux cases
 * @param Les deux questions en questions
 * @return Un entier (-1, +1)
 */
int CompareCases(CASE case1,CASE case2)
{
	if (case1.ligne < case2.ligne) return -1;
	if (case1.ligne > case2.ligne) return +1;
	if (case1.colonne < case2.colonne) return -1;
	if (case1.colonne > case2.colonne) return +1;
	return 0;
}

/*
 * @brief Fonction qui se charge de trier les cases contenu dans un vecteur
 * @param Un pointeur de structure de CASE, deux entiers : l'indice de début et l'indice de fin
 */
void TriCases(CASE *vecteur,int indiceDebut,int indiceFin)
{
	// trie les cases de vecteur entre les indices indiceDebut et indiceFin compris
	// selon le critere impose par la fonction CompareCases()
	// Exemple : pour trier un vecteur v de 4 cases, il faut appeler TriCases(v,0,3); 
	int  i,iMin;
	CASE tmp;

	if (indiceDebut >= indiceFin) return;

	// Recherche du minimum
	iMin = indiceDebut;
	for (i=indiceDebut ; i<=indiceFin ; i++)
	if (CompareCases(vecteur[i],vecteur[iMin]) < 0) iMin = i;

	// On place le minimum a l'indiceDebut par permutation
	tmp = vecteur[indiceDebut];
	vecteur[indiceDebut] = vecteur[iMin];
	vecteur[iMin] = tmp;

	// Tri du reste du vecteur par recursivite
	TriCases(vecteur,indiceDebut+1,indiceFin);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * @brief Fonction qui nous permet de connaitre le numéro du carré en fonction de la position de la case
 * @param  Un pointeur de structure de CASE qui est la position de la case actuel
 * @return un entier qui correspond au numéro du carré
 */
int RechercheCarre(CASE *c)
{
	int a = c->ligne / 3;

	switch(c->colonne / 3)
	{
		case 0: 	
			if(a == 0)
			{
				return 0;
			}
			if(a == 1)
			{
				return 3;
			}
			if(a == 2)
			{
				return 6;
			}

			break;

		case 1:	
			if(a == 0)
			{
			  	return 1;
			}
			if(a == 1)
			{
			  	return 1 + 3;
			}
			if(a == 2)
			{
			  	return 1 + 6;
			}

			break;

		case 2:	
			if(a == 0)
			{
			  	return 2;
			}
			if(a == 1)
			{
			  	return 2 + 3;
			}
			if(a == 2)
			{
			  	return 2 + 6;
			}

			break;
    	}
}


/*
 * @brief Fonction qui se charge d'allouer de l'espace mémoire pour le message et copie son contenu
 * @param Un message et un booléen permettant le SIGALRM 
 */
void setMessage(const char *texte, bool signalOn)
{
	char Espace[5];
	// --- Protection des variables globales concernant le message --- //
	pthread_mutex_lock(&mutexMessage);

	message = (char *) malloc (sizeof(char) + (strlen(texte) + 1));
	strcpy(message, texte);

	indiceCourant = 0;

	// Concaténation du message avec des espaces
	sprintf(Espace, "     ");
	strcat(message, Espace);

	tailleMessage = strlen(message);

	pthread_mutex_unlock(&mutexMessage);

	if(signalOn)
	{
		alarm(10); // On attend 10secondes
	}
}



/**
 * @Brief Fonction qui se charge de récuperer une couleur aléatoire pour une piece
 * @return int couleur
 */
int RandomCouleurPiece(void)
{
  srand(time(NULL));
  
  int CouleursPiece[6] = {ROUGE, VIOLET, VERT, ORANGE, BLEU, JAUNE};
  int indiceCouleur = rand() % 6;
  
  return CouleursPiece[indiceCouleur];
}

/*
 * @brief Fonction qui choisit une pièce au hasard parmi celles fournies dans le vecteur pieces 
 * et stocke dans une variable globale <strong>pieceEnCours</strong> qui represente la pièce suivante à insérer.
 * 
 * @param void
 * @return void
 */
void CreerPiece(void)
{
  pieceEnCours = pieces[rand() % 12];
  pieceEnCours.couleur = RandomCouleurPiece();
}


void RotationPiece(PIECE *pPiece)
{
  // Déclaration des variables de travail
  unsigned short indexCase;
  int LMin = 0, CMin = 0, tmp; 

  for(indexCase = 0; indexCase < pPiece->nbCases; ++indexCase)
  {
  	// Rotation de 90 degré

    tmp = pPiece->cases[indexCase].ligne; 
    pPiece->cases[indexCase].ligne = (pPiece->cases[indexCase].colonne) * -1;
    pPiece->cases[indexCase].colonne = tmp;

    // Recherche de la plus petite ligne et colonne

    if (LMin > pPiece->cases[indexCase].ligne)
      LMin = pPiece->cases[indexCase].ligne;

    if (CMin > pPiece->cases[indexCase].colonne)
      CMin = pPiece->cases[indexCase].colonne;
  }

  // Translation de la pièce pour éviter d'obtenir des valeurs négatives
  for(indexCase = 0; indexCase < pPiece->nbCases; ++indexCase)
  {
    pPiece->cases[indexCase].ligne   = pPiece->cases[indexCase].ligne - LMin;
    pPiece->cases[indexCase].colonne = pPiece->cases[indexCase].colonne - CMin;
  }
}



/**
* @brief Fonction qui se charge de mofifier l'affichage en : "Jeu en cours"
*/
void HandlerSIGALARM(int signal)
{
	//alarm(0); // Si est égale à zéro, toute alarme en attente est annulée.
	fprintf(stderr, "\x1b[33m\x1b[1m[!] On a recu le signal %d (SIGALRM)\x1b[0m\n", signal);
	setMessage("Jeu en cours", false);
}


/**
* @brief Fonction qui permet de vérifier q'une ligne est bien remplie
*/
bool isLigneNotEmpty(CASE *pCase)
{
	
	bool LComplete = true;
	int i = 0;

	while(LComplete && i < 9)
	{
		if(tab[pCase->ligne][i] == VIDE)
		{
			LComplete = false;
		}
		i++;
	}
	return LComplete;
}


/**
* @brief Fonction qui permet de vérifier q'une colonne est bien remplie
*/
bool isColonneNotEmpty(CASE *pCase)
{
	bool CComplete = true;
	int i = 0;

	//  Vérification d'une colonne remplie 
	while(CComplete && i < 9)
	{
		if(tab[i][pCase->colonne] == VIDE)
		{
			CComplete = false;
		}
		i++;
	}

	return CComplete;
}




/*
 * @brief Handler qui se charge de récupérer le signal SIGUSR1 et se charge d'analyser si les lignes, les colonnes et les carrés sont complètes
 * @param le signal
 */
void HandlerSIGUSR1(int sig)
{
	// Déclaration des variables de travail
	CASE *pCase;
	bool LComplete = true, CComplete = true, CarreComplet = true;
	int i = 0, j = 0;
	int NumeroEmplacementSquare;

	// Un thread accède à sa donnée spécifique associé à la clé passée comme paramètre en utilisant la primitive
	pCase = (CASE *) pthread_getspecific(cle);

	// --- Protection des variables globales concernant l'analyse 
	pthread_mutex_lock(&mutexAnalyse);

	// ==================================================================
	// ================ Vérification d'une ligne remplie ================
	// ==================================================================
	LComplete = isLigneNotEmpty(pCase);

	// On rentre dans la condition si la ligne est complete
	if(LComplete)
	{
		i = 0;

		// --- Rechercher d'une place disponible dans le vecteur lignesCompletes --- //
		while(lignesCompletes[i] != -1)
		{
			// --- Vérifie si la ligne est présente dans le vecteur lignesCompletes --- //
			if(lignesCompletes[i] == pCase->ligne)
			{
				LComplete = false;
			}
			i++;
		}

		// On entre dans la condition si la ligne n'est pas présente dans le vecteur lignesCompletes <<< //
		if(LComplete)
		{
			lignesCompletes[i] = pCase->ligne;
			nbLignesCompletes++;

			//  Dessine des briques en fusion pour la ligne qui est complète
			for(i = 0; i < 9; i++)
			{
				DessineBrique(pCase->ligne, i, true);
			}
		}
	}


	// ==================================================================
	// =============== Vérification d'une colonne remplie ===============
	// ==================================================================
	CComplete = isColonneNotEmpty(pCase);

	// On rentre dans la condition si la colonne est complete 
	if(CComplete)
	{
		i = 0;

		// Rechercher d'une place disponible dans le vecteur colonnesCompletes 
		while(colonnesCompletes[i] != -1)
		{
			// Vérifie si la colonne est présente dans le vecteur colonnesCompletes 
			if(colonnesCompletes[i] == pCase->colonne)
			{
				CComplete = false;
			}
			i++;
		}

		// On entre dans la condition si la colonne n'est pas présent dans le vecteur colonnesCompletes 
		if(CComplete)
		{
			colonnesCompletes[i] = pCase->colonne;
			nbColonnesCompletes++;

			//  Dessine des briques en fusion pour la colonne qui est complète 
			for(i = 0; i < 9; i++)
			{
				DessineBrique(i, pCase->colonne, true);
			}
		}
	}


	// ==================================================================
	// =============== Vérification d'un carre remplie ===============
	// ==================================================================
	NumeroEmplacementSquare = RechercheCarre(pCase);

	// Vérification d'un carré rempli
	for(i = MinSquares[NumeroEmplacementSquare / 3]; i <= MaxSquares[NumeroEmplacementSquare / 3]; i++)
	{
		for(j = MinSquares[NumeroEmplacementSquare % 3]; j <= MaxSquares[NumeroEmplacementSquare % 3]; j++)
		{
			if(tab[i][j] == VIDE)
			{
				CarreComplet = false;
				j = MaxSquares[NumeroEmplacementSquare % 3] + 1;
				i = MaxSquares[NumeroEmplacementSquare / 3] + 1;
			}
		}
	}


	// On entre dans la condition si le carré est rempli
	if(CarreComplet)
	{
		i = 0;

		// Recherche d'une place libre dans le vecteur carresComplets
		while(carresComplets[i] != -1)
		{
			// Vérifie si le numéro du carré n'est pas déjà présent dans le vecteur
			if(carresComplets[i] == NumeroEmplacementSquare)
			{
				CarreComplet = false;
			}
			i++;
		}

		//  On entre dans la condition si le numéro du carré n'est pas présent dans le vecteur 
		if(CarreComplet)
		{
			carresComplets[i] = NumeroEmplacementSquare;
			nbCarresComplets++;

			// --- Dessine plusieurs briques à l'emplace du numéro du carré --- //
			for(i = MinSquares[NumeroEmplacementSquare / 3]; i <= MaxSquares[NumeroEmplacementSquare / 3]; i++)
			{
				for(j = MinSquares[NumeroEmplacementSquare % 3]; j <= MaxSquares[NumeroEmplacementSquare % 3]; j++)
				{
					DessineBrique(i, j, true);
				}
			}
		}
	}


	pthread_mutex_unlock(&mutexAnalyse);

	pthread_mutex_lock(&mutexAnalyse);
	nbAnalyses++; // Incrémentation du nombre d'analyse lorsque l'analyse est terminée 
	pthread_mutex_unlock(&mutexAnalyse);

	pthread_cond_signal(&condAnalyse); // Reveille le threadNettoyeur
}
