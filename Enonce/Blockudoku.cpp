#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include "GrilleSDL.h"
#include "Ressources.h"

// Dimensions de la grille de jeu
#define NB_LIGNES   12
#define NB_COLONNES 19

// Nombre de cases maximum par piece
#define NB_CASES    4

// Macros utilisees dans le tableau tab
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

int  CompareCases(CASE case1,CASE case2);
void TriCases(CASE *vecteur,int indiceDebut,int indiceFin);


/* === DECLARATION DES VARIABLES DE TRAVAIL == */

#define handle_error(msg) \
          do { perror(msg); exit(EXIT_FAILURE); } while (0)
#define TAILLE_BANNER 17

    
pthread_t thHandler;


struct timespec monTimer;
          
// === Déclaration des mutex ===
pthread_mutex_t mutexMessage         = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexScore           = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexCasesInserees   = PTHREAD_MUTEX_INITIALIZER;

// === Déclaration des variables condition ===
pthread_cond_t	condScore;
pthread_cond_t  condCasesInserees;



int lignesCompletes[NB_CASES];
int nbLignesCompletes;
int colonnesCompletes[NB_CASES];
int nbColonnesCompletes;
int carresComplets[NB_CASES];
int nbCarresComplets;
int nbAnalyses;

pthread_key_t cle;

pthread_t threadHandle;
pthread_t tabThreadCase[9][9];

// === Les variables de travail ===
char* message;		// Pointeur vers le message à faire défiler
int   tailleMessage;	// Longueur du message
int   indiceCourant;	// Indice du premier caractère à afficher dans la zone graphique

int  score		  = 0;
bool MAJScore	  = true;


PIECE pieceEnCours; 
int   nbCasesInserees = 0; // nombre de cases actuellement insérées par le joueur
CASE  casesInserees[NB_CASES]; // cases insérées par le joueur


// === Les prototypes pour les threads ===
void  *threadDefileMessage(void);
void  *threadFin(void);
void  *threadEvent(void);
void  *threadScore(void);
void  *threadEvent(void);
void  *threadPiece(void);
void  *threadCases(CASE *);

// === Les prototypes des fonctions ===
void  HandlerSIGALARM(int sig);
void  HandlerSIGUSR1(int sig);
void  HandlerClickDroit(int sig);



void  setMessage(const char *, bool);



void  CreerPiece();
int   RandomCouleurPiece(void);
void  RotationPiece(PIECE *pPiece);


///////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc,char* argv[])
{
  
  /* ------------------------------------------------------------
   DECLARATION POUR L'UTILISATION DES SIGNAUX
   ------------------------------------------------------------ */
  unsigned short index = 0;
  int l = 0, c = 0;
  CASE *pCase = NULL;
  struct sigaction sigact;
  
  
  // ======== Signal SIGALRM ========
  sigact.sa_handler = HandlerSIGALARM;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;

  if ((sigaction(SIGALRM, &sigact, NULL)) == -1)
    handle_error("Erreur d'armement du signal SIGALRM");
  

  // ======== Traitement ========
  EVENT_GRILLE_SDL event;
  srand((unsigned)time(NULL));
  
  
  sigset_t mask;
  sigfillset(&mask); // On masque l'ensemble des signaux
  sigprocmask(SIG_SETMASK, &mask, NULL);	
  
  // === Initialise le mutex avec les parametres par defaut ===
  pthread_mutex_init(&mutexMessage, NULL);
  pthread_mutex_init(&mutexCasesInserees, NULL);
  pthread_mutex_init(&mutexScore, NULL);
  
  
  // === Initialisation des conditions ===
  pthread_cond_init(&condScore, NULL);
  pthread_cond_init(&condCasesInserees, NULL);
  

  

  // Ouverture de la fenetre graphique
  printf("(MAIN %d) Ouverture de la fenetre graphique\n",pthread_self()); fflush(stdout);
  if (OuvertureFenetreGraphique() < 0)
  {
    printf("Erreur de OuvrirGrilleSDL\n");
    fflush(stdout);
    exit(1);
  }

  
  // === Creation des threads ===
  pthread_create(&thHandler, NULL, (void * (*) (void *))threadDefileMessage, NULL);
  pthread_create(&thHandler, NULL, (void * (*) (void *))threadEvent,         NULL);
  pthread_create(&thHandler, NULL, (void * (*) (void *))threadPiece,         NULL);
  pthread_create(&thHandler, NULL, (void * (*) (void *))threadScore,         NULL);
  
  setMessage("Bienvenue sur Blockudoku", true);


  for(l = 0; l < 9; l++)
  {
    for(c = 0; c < 9; c++)
    {
      pCase = (CASE *) malloc(sizeof(CASE));
      pCase->ligne = l;
      pCase->colonne = c;
      phtrad_create(&tabThreadCase[l][c], NULL, (void *(*)(void *))threadCases, CaseThread);
    }
  }

  for(index = 0; index < NB_CASES; ++index)
  {
    lignesC
  }


  while(1);

  // Fermeture de la fenetre
  printf("(MAIN %d) Fermeture de la fenetre graphique...",pthread_self()); fflush(stdout);
  FermetureFenetreGraphique();
  printf("OK\n");

  exit(0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int CompareCases(CASE case1,CASE case2)
{
  if (case1.ligne < case2.ligne) return -1;
  if (case1.ligne > case2.ligne) return +1;
  if (case1.colonne < case2.colonne) return -1;
  if (case1.colonne > case2.colonne) return +1;
  return 0;
}

void TriCases(CASE *vecteur,int indiceDebut,int indiceFin)
{ // trie les cases de vecteur entre les indices indiceDebut et indiceFin compris
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
    tmp = pPiece->cases[indexCase].ligne; 
    pPiece->cases[indexCase].ligne = (pPiece->cases[indexCase].colonne) * -1;
    pPiece->cases[indexCase].colonne = tmp;

    if (LMin > pPiece->cases[indexCase].ligne)
      LMin = pPiece->cases[indexCase].ligne;

    if (CMin > pPiece->cases[indexCase].colonne)
      CMin = pPiece->cases[indexCase].colonne;
  }

  for(indexCase = 0; indexCase < pPiece->nbCases; ++indexCase)
  {
    pPiece->cases[indexCase].ligne   = pPiece->cases[indexCase].ligne - LMin;
    pPiece->cases[indexCase].colonne = pPiece->cases[indexCase].colonne - CMin;
  }
}



void * threadPiece(void)
{

  srand(time(NULL));

  // Déclaration des variables de travail
  int i, LMin, CMin;
  
  bool estSimilairePiece; // Piece en cours = Piece insérée
  unsigned short nbRotations = 0, indexRotation = 0;
  unsigned short MainRunning = 1, SecondRunning;


  sigset_t mask;
  sigfillset(&mask); // On masque l'ensemble des signaux
  sigprocmask(SIG_SETMASK, &mask, NULL);

  

  while(MainRunning)
  {
    CreerPiece();

    nbRotations = rand() % 4; // 0 -> 3
    while(indexRotation < nbRotations)
    {
      RotationPiece(&pieceEnCours);
      indexRotation++;
    }

    TriCases(pieceEnCours.cases, 0, pieceEnCours.nbCases - 1);

    // ==== [Placement de la pièce dans la zone C pour l'affichage] ===
    for(i = 0; i < pieceEnCours.nbCases; ++i)
      DessineDiamant(pieceEnCours.cases[i].ligne + 3, pieceEnCours.cases[i].colonne + 14, pieceEnCours.couleur);


    SecondRunning = 1;

    while(SecondRunning)
    {
      estSimilairePiece = true;

      // --- Attend que l'utilisateur insère des diamants égale au nombre de case de la pièce dans la zone C 
      pthread_mutex_lock(&mutexCasesInserees);
      while(nbCasesInserees < pieceEnCours.nbCases)
        pthread_cond_wait(&condCasesInserees, &mutexCasesInserees); // Attendre d'être wake up par le le threadEvent
      pthread_mutex_unlock(&mutexCasesInserees);

      // === [Déterminer le LMin et le CMin de la pièce inséré par l'utilisateur] ===
      LMin = casesInserees[0].ligne;
      CMin = casesInserees[0].colonne;

      for(i = 1; i < nbCasesInserees; ++i)
      {
        if(LMin > casesInserees[i].ligne)
          LMin = casesInserees[i].ligne;

        if(CMin > casesInserees[i].colonne)
          CMin = casesInserees[i].colonne;
      }

      pthread_mutex_lock(&mutexCasesInserees);

      // --- Suppression de LMin et CMin pour chaque ligne et colonne
      for(i = 0; i < nbCasesInserees; i++)
      {
        casesInserees[i].ligne -= LMin;
        casesInserees[i].colonne -= CMin;
      }

      // --- Tri des différentes cases de la pièce insérée par l'utilisateur
      TriCases(casesInserees, 0, nbCasesInserees - 1);

      pthread_mutex_unlock(&mutexCasesInserees);

      // --- Vérification de la pièce insérée par l'utilisateur par rapport à la pièce en cours dans la zone C
      for(i = 0; i < nbCasesInserees; ++i)
      {
        if( (casesInserees[i].ligne != pieceEnCours.cases[i].ligne) || (casesInserees[i].colonne != pieceEnCours.cases[i].colonne) )
          estSimilairePiece = false;
      }


      if(estSimilairePiece)
      {
        // --- Dessine la pièce insérée par l'utilisateur en brique bleu --- //
        for(i = 0; i < nbCasesInserees; i++)
        {
          tab[casesInserees[i].ligne + LMin][casesInserees[i].colonne + CMin] = BRIQUE;
          DessineBrique(casesInserees[i].ligne + LMin, casesInserees[i].colonne + CMin, false);
        }

        // --- Suppression de la pièce dans la zone C --- //
        for(i = 0; i < pieceEnCours.nbCases; i++)
        {
          EffaceCarre(pieceEnCours.cases[i].ligne + 3, pieceEnCours.cases[i].colonne + 14);
        }


        pthread_mutex_lock(&mutexScore);
        score += pieceEnCours.nbCases;
        MAJScore = true;
        pthread_mutex_unlock(&mutexScore);

        pthread_cond_signal(&condScore); // Réveiller le threadScore

        SecondRunning = 0;

      } else { // Dans le cas ou la pièce insérée n'est pas la même que la pièce en cours !

        // Dessine la pièce insérée par l'utilisateur en brique bleu 
        for(i = 0; i < nbCasesInserees; i++)
        {
          tab[casesInserees[i].ligne + LMin][casesInserees[i].colonne + CMin] = VIDE;
          EffaceCarre(casesInserees[i].ligne + LMin, casesInserees[i].colonne + CMin);
        }

        // Suppression des lignes et colonnes dans les casesInserees
        for(i = 0; i < nbCasesInserees; i++)
        {
          casesInserees[i].ligne = VIDE;
          casesInserees[i].colonne = VIDE;
        }

      } // FIN ELSE

      pthread_mutex_lock(&mutexCasesInserees);
      nbCasesInserees = 0;
      pthread_mutex_unlock(&mutexCasesInserees);

    }
  }
  pthread_exit(NULL);
}


/**
 * @brief Fonction permettant d'envoyer un signal suivant le click de la souris.
 */
void* threadEvent(void)
{

  
  // Déclaration des variables de travail
  sigset_t mask;
  sigfillset(&mask); // On masque l'ensemble des signaux
  sigprocmask(SIG_SETMASK, &mask, NULL);
  
  unsigned short MainRunning = 1;
  unsigned short i = 0;
  EVENT_GRILLE_SDL event;
  
  
  while(MainRunning)
  {
    event = ReadEvent();
    
    if(event.type == CLIC_GAUCHE)
    {
      if((tab[event.ligne][event.colonne] == VIDE))
      {
        // --- Remplissage de l'affichage + dans la matrice des cases insérées --- //
        DessineDiamant(event.ligne,event.colonne,ROUGE);
        tab[event.ligne][event.colonne] = DIAMANT;

        // --- Protege les variables gobales concernant les cases insérées --- //
        pthread_mutex_lock(&mutexCasesInserees);

        casesInserees[nbCasesInserees].ligne = event.ligne;
        casesInserees[nbCasesInserees].colonne = event.colonne;

        nbCasesInserees++;

        pthread_mutex_unlock(&mutexCasesInserees);

        // --- Reveille le threadPiece --- //
        pthread_cond_signal(&condCasesInserees);
      }
    }

    if(event.type == CLIC_DROIT) // Supprimer DIAMANT
    {

      // --- Suppression de l'affichage + dans la matrice des cases insérées --- //
      for(i = 0; i < nbCasesInserees; i++)
      {
        EffaceCarre(casesInserees[i].ligne, casesInserees[i].colonne);
        tab[casesInserees[i].ligne][casesInserees[i].colonne] = VIDE;
      }
      
      // --- Protege les variables gobales concernant les cases insérées --- //
      pthread_mutex_lock(&mutexCasesInserees);

      // --- Suppression des lignes et colonnes dans les casesInserees --- //
      for(i = 0; i < nbCasesInserees; i++)
      {
        casesInserees[i].ligne = VIDE;
        casesInserees[i].colonne = VIDE;
      }
  
      nbCasesInserees = 0;
      
      pthread_mutex_unlock(&mutexCasesInserees);
    }
    
    if(event.type == CROIX) {
      fprintf(stderr, "\x1b[32mVous venez de quitter le jeu !\n\x1b[0m");
      kill(getpid(), SIGKILL);
      MainRunning = 0;
    }
    
  }
  
  pthread_exit(NULL);
}



void* threadScore(void)
{
  sigset_t mask;
  sigfillset(&mask); // On masque l'ensemble des signaux
  sigprocmask(SIG_SETMASK, &mask, NULL);
  
  while(1)
  {
    pthread_mutex_lock(&mutexScore); // Locking

    while(!MAJScore)
    {
      pthread_cond_wait(&condScore, &mutexScore); // Attendre d'être wake up par le threadPiece
    }

    DessineChiffre(1, 14, score / 1000);
    DessineChiffre(1, 15, (score % 1000) / 100);
    DessineChiffre(1, 16, (score % 100) / 10);
    DessineChiffre(1, 17, score % 10);
   
    MAJScore = false;
    pthread_mutex_unlock(&mutexScore);
  }
}


void* threadDefileMessage(void)
{
  /* ------------------------------------------------------------
   DECLARATION POUR L'UTILISATION DES SIGNAUX
   ------------------------------------------------------------ */
  struct sigaction sigact;
  
  sigact.sa_handler = HandlerSIGALARM;
  sigact.sa_flags = 0;
  
  
  sigfillset(&sigact.sa_mask); // On masque l'ensemble des signaux
  sigdelset(&sigact.sa_mask, SIGALRM); // On retire du masquage le signal SIGALRM
  
  sigprocmask(SIG_SETMASK, &sigact.sa_mask, NULL);
  if ((sigaction(SIGALRM, &sigact, 0))==-1)
    handle_error( "Erreur : sigaction" );
  
  
  
  // === DECLARATION DES VARIABLES DE TRAVAIL ===
  int positionBuffer;

 
  
  // L'empilement d'un appel à une fonction de terminaison
  pthread_cleanup_push((void (*) (void *))threadFin, 0);
  
  monTimer.tv_sec = 0;
  monTimer.tv_nsec = 400000000; // 0.4 secondes
  

  // === TRAITEMENT ===
  while(1)
  {
    for(positionBuffer = 0; positionBuffer < TAILLE_BANNER; ++positionBuffer)
    {
  	  DessineLettre(10, positionBuffer + 1, *(message + ((indiceCourant + positionBuffer) % tailleMessage) ));
    }
    nanosleep(&monTimer, NULL);
    indiceCourant++;
  }

  // L'appel de la fonction en fin d'execution du thread n'est pas automatique, il doit être provoqué par l'appel de la fonction dépilement
  pthread_cleanup_pop(1);
  
  pthread_exit(NULL);
}

void *threadFin(void)
{
  free(message);
}


void setMessage(const char *texte, bool signalOn)
{
  pthread_mutex_lock(&mutexMessage); // Lock du mutexMessage

  
  indiceCourant = 0; // Initialiser l'indiceCourant
  message = (char *)malloc(sizeof(char) * (strlen(texte) + 1));
  strcpy(message, texte);
  
  // + Ajout des espaces
  char lesEspaces[5];
  memset(lesEspaces, ' ', 5);
  strcat(message, lesEspaces);
  
  tailleMessage = strlen(message); // Recuperation de la taille
  
  pthread_mutex_unlock(&mutexMessage); // unlock du mutexMessage
  
  if(signalOn)
    alarm(10); // On attend 10 secondes
}


/**
* @brief Fonction qui se charge de mofifier l'affichage en : "Jeu en cours"
*/
void HandlerSIGALARM(int unused)
{
  alarm(0); // Si la seconde est égale à zéro, toute alarme en attente est annulée.
  setMessage("Jeu en cours", false);
}




void *threadCases(CASE *pCase)
{

  pthread_setspecific(cle, (CASE *)pCase);


  struct sigaction sigact;  
  sigact.sa_handler = HandlerSIGUSR1;
  sigact.sa_flags = 0;
  
  
  sigfillset(&sigact.sa_mask); // On masque l'ensemble des signaux
  sigdelset(&sigact.sa_mask, SIGUSR1); // On retire du masquage le signal SIGUSR1
  
  sigprocmask(SIG_SETMASK, &sigact.sa_mask, NULL);
  if ((sigaction(SIGALRM, &sigact, 0))==-1)
    handle_error( "Erreur : sigaction" );

  while(1) {
    pause();
  } 

  pthread_exit(NULL);
}



void HandlerSIGUSR1(int unused)
{
  // Déclaration des variables de travail
  bool estRempli = true;
  unsigned int index = 0;
  CASE *pCase;

  pCase = (CASE *) pthread_getspecific(cle);


  for(index = 0; estRempli && index < 9; ++index)
  {
    if(tab[pCase->ligne][index] == VIDE)
      estRempli = false;
  }


}

void HandlerClickDroit(int unused)
{
  
  
}


