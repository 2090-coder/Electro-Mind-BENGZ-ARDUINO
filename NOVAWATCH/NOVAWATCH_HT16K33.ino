#include <Wire.h>

// ============================================================
// NOVAWATCH
// Arduino Nano + HT16K33 + DS3231 + 7x74HC595 + 7xULN2803A
// Affichage a 4 chiffres et 7 segments, CATHODE COMMUNE
// Alimentation d entree: 12 V DC (utiliser un regulateur abaisseur 5 V adapte)
// ============================================================

// -------------------- BOUTONS / AVERTISSEUR -----------------------
const byte PIN_BOUTON_ALIMENTATION = 2;
const byte PIN_BOUTON_MODE  = 3;
const byte PIN_BOUTON_PLUS  = 4;
const byte PIN_BOUTON_MOINS = 5;
const byte PIN_AVERTISSEUR       = 6;
const byte PIN_DEUX_POINTS        = 7;

// -------------------- CONTOUR 74HC595 ------------------------
const byte PIN_595_DONNEES  = 8;
const byte PIN_595_VERROU = 9;
const byte PIN_HORLOGE     = 13;

// -------------------- CIRCUIT I2C ----------------------------
const byte RTC_ADRESSE_I2C = 0x68;
const byte HT16K33_ADRESSE_I2C = 0x70;

const byte HT_LIGNE0 = 0;
const byte HT_LIGNE1 = 1;
const byte HT_LIGNE2 = 2;
const byte HT_LIGNE3 = 3;
const byte HT_LIGNE4 = 4;
const byte HT_LIGNE5 = 5;
const byte HT_LIGNE6 = 6;
const byte HT_LIGNE7 = 7;

const byte HT_COMMUN0 = 0;
const byte HT_COMMUN1 = 1;
const byte HT_COMMUN2 = 2;
const byte HT_COMMUN3 = 3;

// Commandes HT16K33
const byte HT_COMMANDE_SYSTEME_ACTIVE    = 0x21;
const byte HT_COMMANDE_AFFICHAGE_ACTIVE   = 0x81;
const byte HT_COMMANDE_LUMINOSITE   = 0xE0;


// -------------------- ETAT GENERAL DE LA MONTRE ---------------------
bool montreActive = false;
bool demarrageActif = false;
unsigned long debutDemarrage = 0;
unsigned long derniereImageDemarrage = 0;
byte imageDemarrage = 0;

const unsigned long DUREE_DEMARRAGE = 2300;
const unsigned long INTERVALLE_IMAGE_DEMARRAGE = 120;

enum ChampReglage { REGLAGE_HEURE, REGLAGE_MINUTE };
bool modeReglage = false;
ChampReglage champReglage = REGLAGE_HEURE;
byte heureReglage = 0;
byte minuteReglage = 0;
bool reglageVisible = true;
unsigned long dernierClignotement = 0;
const unsigned long INTERVALLE_CLIGNOTEMENT = 350;

// -------------------- CARTE DES 7 SEGMENTS --------------------------
// Ces bits correspondent aux lignes ROW0..ROW7 du HT16K33.
  // Sur le module, ces sorties sont reperees A0..A7.
// Brancher l affichage selon les reperes du module :
// ROW0=A, ROW1=B, ROW2=C, ROW3=D,
// ROW4=E, ROW5=F, ROW6=G, ROW7=DP.
const byte HT_LIGNE0_SEG_A  = 0x01;
const byte HT_LIGNE1_SEG_B  = 0x02;
const byte HT_LIGNE2_SEG_C  = 0x04;
const byte HT_LIGNE3_SEG_D  = 0x08;
const byte HT_LIGNE4_SEG_E  = 0x10;
const byte HT_LIGNE5_SEG_F  = 0x20;
const byte HT_LIGNE6_SEG_G  = 0x40;
const byte HT_LIGNE7_SEG_DP = 0x80;

const byte HT_MASQUE_CHIFFRE[10] = {
  HT_LIGNE0_SEG_A | HT_LIGNE1_SEG_B | HT_LIGNE2_SEG_C | HT_LIGNE3_SEG_D | HT_LIGNE4_SEG_E | HT_LIGNE5_SEG_F,                    // 0
  HT_LIGNE1_SEG_B | HT_LIGNE2_SEG_C,                                                    // 1
  HT_LIGNE0_SEG_A | HT_LIGNE1_SEG_B | HT_LIGNE3_SEG_D | HT_LIGNE4_SEG_E | HT_LIGNE6_SEG_G,                            // 2
  HT_LIGNE0_SEG_A | HT_LIGNE1_SEG_B | HT_LIGNE2_SEG_C | HT_LIGNE3_SEG_D | HT_LIGNE6_SEG_G,                            // 3
  HT_LIGNE1_SEG_B | HT_LIGNE2_SEG_C | HT_LIGNE5_SEG_F | HT_LIGNE6_SEG_G,                                    // 4
  HT_LIGNE0_SEG_A | HT_LIGNE2_SEG_C | HT_LIGNE3_SEG_D | HT_LIGNE5_SEG_F | HT_LIGNE6_SEG_G,                            // 5
  HT_LIGNE0_SEG_A | HT_LIGNE2_SEG_C | HT_LIGNE3_SEG_D | HT_LIGNE4_SEG_E | HT_LIGNE5_SEG_F | HT_LIGNE6_SEG_G,                    // 6
  HT_LIGNE0_SEG_A | HT_LIGNE1_SEG_B | HT_LIGNE2_SEG_C,                                            // 7
  HT_LIGNE0_SEG_A | HT_LIGNE1_SEG_B | HT_LIGNE2_SEG_C | HT_LIGNE3_SEG_D | HT_LIGNE4_SEG_E | HT_LIGNE5_SEG_F | HT_LIGNE6_SEG_G,            // 8
  HT_LIGNE0_SEG_A | HT_LIGNE1_SEG_B | HT_LIGNE2_SEG_C | HT_LIGNE3_SEG_D | HT_LIGNE5_SEG_F | HT_LIGNE6_SEG_G                     // 9
};

// Correspondance de la RAM d affichage du HT16K33 :
// COM0 -> RAM 0x00/0x01
// COM1 -> RAM 0x02/0x03
// COM2 -> RAM 0x04/0x05
// COM3 -> RAM 0x06/0x07
// pour a 4-chiffre affichage, seul the premier octet of each paire est needed.

void ht16k33Commande(byte commande) {
  Wire.beginTransmission(HT16K33_ADRESSE_I2C);
  Wire.write(commande);
  Wire.endTransmission();
}

void ht16k33Effacer() {
  Wire.beginTransmission(HT16K33_ADRESSE_I2C);
  Wire.write((byte)0x00); // pointeur de RAM d affichage
  for (byte i = 0; i < 16; i++) {
    Wire.write((byte)0x00);
  }
  Wire.endTransmission();
}

void ht16k33Initialiser() {
  ht16k33Commande(HT_COMMANDE_SYSTEME_ACTIVE);  // oscillateur interne ACTIVE
  ht16k33Effacer();

  // Affichage ACTIVE, clignotement DESACTIVE
  ht16k33Commande(HT_COMMANDE_AFFICHAGE_ACTIVE);

  // Luminosite de 0 a 15. 8 est une valeur de depart raisonnable.
  ht16k33Commande(HT_COMMANDE_LUMINOSITE | 0x08);
}

// Ecrit un motif de segments de 8 bits sur ROW0..ROW7 pour une ligne COM.
void ht16k33EcrireChiffre(byte chiffreIndex, byte segments) {
  if (chiffreIndex > 3) return;

  const byte HT_COM_RAM_ADDRESS[4] = {0x00, 0x02, 0x04, 0x06};
  byte adresseRam = HT_COM_RAM_ADDRESS[chiffreIndex];

  Wire.beginTransmission(HT16K33_ADRESSE_I2C);
  Wire.write(adresseRam);
  Wire.write(segments);
  Wire.write((byte)0x00); // deuxieme octet = ROW8..ROW15, inutilise ici
  Wire.endTransmission();
}

void ht16k33AfficherHeure(byte h, byte m) {
  ht16k33EcrireChiffre(0, HT_MASQUE_CHIFFRE[h / 10]);
  ht16k33EcrireChiffre(1, HT_MASQUE_CHIFFRE[h % 10]);
  ht16k33EcrireChiffre(2, HT_MASQUE_CHIFFRE[m / 10]);
  ht16k33EcrireChiffre(3, HT_MASQUE_CHIFFRE[m % 10]);
}

void ht16k33AfficherReglage() {
  byte dizaineHeure = heureReglage / 10;
  byte uniteHeure = heureReglage % 10;
  byte dizaineMinute = minuteReglage / 10;
  byte uniteMinute = minuteReglage % 10;

  if (champReglage == REGLAGE_HEURE && !reglageVisible) {
    ht16k33EcrireChiffre(0, 0);
    ht16k33EcrireChiffre(1, 0);
  } else {
    ht16k33EcrireChiffre(0, HT_MASQUE_CHIFFRE[dizaineHeure]);
    ht16k33EcrireChiffre(1, HT_MASQUE_CHIFFRE[uniteHeure]);
  }

  if (champReglage == REGLAGE_MINUTE && !reglageVisible) {
    ht16k33EcrireChiffre(2, 0);
    ht16k33EcrireChiffre(3, 0);
  } else {
    ht16k33EcrireChiffre(2, HT_MASQUE_CHIFFRE[dizaineMinute]);
    ht16k33EcrireChiffre(3, HT_MASQUE_CHIFFRE[uniteMinute]);
  }
}

void reglerDeuxPoints(bool active) {
  chiffrealWrite(PIN_DEUX_POINTS, active ? HIGH : LOW);
}

// -------------------- DS3231 --------------------------------
byte bcdVersDecimal(byte valeur) {
  return ((valeur >> 4) * 10) + (valeur & 0x0F);
}

byte decimalVersBcd(byte valeur) {
  return ((valeur / 10) << 4) | (valeur % 10);
}

byte heureActuelle = 0;
byte minuteActuelle = 0;
byte secondeActuelle = 0;
unsigned long derniereLectureRTC = 0;
const unsigned long INTERVALLE_LECTURE_RTC = 500;

bool rtcLireHeure() {
  Wire.beginTransmission(RTC_ADRESSE_I2C);
  Wire.write((byte)0x00);
  if (Wire.endTransmission() != 0) return false;

  if (Wire.requestFrom(RTC_ADRESSE_I2C, (byte)3) != 3) return false;

  secondeActuelle = bcdVersDecimal(Wire.read() & 0x7F);
  minuteActuelle = bcdVersDecimal(Wire.read() & 0x7F);
  heureActuelle   = bcdVersDecimal(Wire.read() & 0x3F);

  if (heureActuelle > 23 || minuteActuelle > 59 || secondeActuelle > 59) return false;
  return true;
}

bool rtcEcrireHeure(byte h, byte m, byte s) {
  if (h > 23 || m > 59 || s > 59) return false;

  Wire.beginTransmission(RTC_ADRESSE_I2C);
  Wire.write((byte)0x00);
  Wire.write(decimalVersBcd(s));
  Wire.write(decimalVersBcd(m));
  Wire.write(decimalVersBcd(h));
  return Wire.endTransmission() == 0;
}

// -------------------- CONTOUR : 7x74HC595 ---------------------
const byte NOMBRE_REGISTRES_DECALAGE = 7;
const byte GROUPES_PAR_COULEUR = 17;
const byte NOMBRE_GROUPES_CONTOUR = 51;
byte donneesContour[NOMBRE_REGISTRES_DECALAGE];

const unsigned long INTERVALLE_CONTOUR = 100;
unsigned long derniereMiseAJourContour = 0;
byte positionContour = 0;
byte couleurContour = 0;

void effacerContour() {
  for (byte i = 0; i < NOMBRE_REGISTRES_DECALAGE; i++) {
    donneesContour[i] = 0;
  }
}

void reglerGroupeContour(byte groupe, bool active) {
  if (groupe >= NOMBRE_GROUPES_CONTOUR) return;

  byte circuit = groupe / 8;
  byte bit = groupe % 8;

  if (active) {
    donneesContour[circuit] |= (byte)(1 << bit);
  } else {
    donneesContour[circuit] &= (byte)~(1 << bit);
  }
}

void ecrireContour() {
  chiffrealWrite(PIN_595_VERROU, LOW);

  for (int circuit = NOMBRE_REGISTRES_DECALAGE - 1; circuit >= 0; circuit--) {
    shiftOut(PIN_595_DONNEES, PIN_HORLOGE, LSBFIRST, donneesContour[circuit]);
  }

  chiffrealWrite(PIN_595_VERROU, HIGH);
}

void initialiserContour() {
  pinMode(PIN_595_DONNEES, OUTPUT);
  pinMode(PIN_595_VERROU, OUTPUT);
  pinMode(PIN_HORLOGE, OUTPUT);

  effacerContour();
  ecrireContour();
}

void mettreAJourContour() {
  if (!montreActive) return;
  if (millis() - derniereMiseAJourContour < INTERVALLE_CONTOUR) return;

  derniereMiseAJourContour = millis();

  effacerContour();

  byte groupe = positionContour + (couleurContour * GROUPES_PAR_COULEUR);
  reglerGroupeContour(groupe, true);
  ecrireContour();

  couleurContour++;
  if (couleurContour >= 3) {
    couleurContour = 0;
    positionContour++;
    if (positionContour >= GROUPES_PAR_COULEUR) positionContour = 0;
  }
}

// -------------------- AVERTISSEUR / DEMARRAGE -----------------------
void bipAction() {
  tone(PIN_AVERTISSEUR, 880, 60);
}

void bipChiffre(byte chiffre) {
  const unsigned int frequences[10] = {
    262, 294, 330, 349, 392, 440, 494, 523, 587, 659
  };
  tone(PIN_AVERTISSEUR, frequences[chiffre % 10], 75);
}

struct NoteMusicale {
  unsigned int frequence;
  unsigned int duree;
};

const NoteMusicale MELODIE_DEMARRAGE[] = {
  {523, 120}, {659, 120}, {784, 120}, {1047, 220},
  {784, 120}, {659, 120}, {523, 260}
};

const byte MELODIE_DEMARRAGE_COUNT =
  sizeof(MELODIE_DEMARRAGE) / sizeof(MELODIE_DEMARRAGE[0]);

byte indexMelodie = 0;
unsigned long prochaineNote = 0;

void demarrerMelodie() {
  indexMelodie = 0;
  prochaineNote = 0;
}

void mettreAJourMelodie() {
  if (!demarrageActif) return;

  unsigned long maintenant = millis();
  if (maintenant < prochaineNote) return;

  if (indexMelodie >= MELODIE_DEMARRAGE_COUNT) {
    noTone(PIN_AVERTISSEUR);
    prochaineNote = maintenant + 100000UL;
    return;
  }

  tone(
    PIN_AVERTISSEUR,
    MELODIE_DEMARRAGE[indexMelodie].frequence,
    MELODIE_DEMARRAGE[indexMelodie].duree - 10
  );

  prochaineNote = maintenant + MELODIE_DEMARRAGE[indexMelodie].duree;
  indexMelodie++;
}

void afficherEcranDemarrage(byte image) {
  byte masque = 0;

  switch (image % 8) {
    case 0: masque = HT_LIGNE0_SEG_A; break;
    case 1: masque = HT_LIGNE1_SEG_B; break;
    case 2: masque = HT_LIGNE2_SEG_C; break;
    case 3: masque = HT_LIGNE3_SEG_D; break;
    case 4: masque = HT_LIGNE4_SEG_E; break;
    case 5: masque = HT_LIGNE5_SEG_F; break;
    case 6: masque = HT_LIGNE6_SEG_G; break;
    case 7: masque = HT_LIGNE0_SEG_A | HT_LIGNE1_SEG_B | HT_LIGNE2_SEG_C | HT_LIGNE3_SEG_D |
                      HT_LIGNE4_SEG_E | HT_LIGNE5_SEG_F | HT_LIGNE6_SEG_G; break;
  }

  ht16k33EcrireChiffre(0, masque);
  ht16k33EcrireChiffre(1, masque);
  ht16k33EcrireChiffre(2, masque);
  ht16k33EcrireChiffre(3, masque);

  reglerDeuxPoints((image % 2) == 0);
}

// -------------------- ANTI-REBOND DES BOUTONS ------------------------
struct Bouton {
  byte pin;
  bool lectureBrute;
  bool etatStable;
  unsigned long dernierChangement;
};

Bouton boutonAlimentation = {PIN_BOUTON_ALIMENTATION, HIGH, HIGH, 0};
Bouton boutonMode  = {PIN_BOUTON_MODE, HIGH, HIGH, 0};
Bouton boutonPlus  = {PIN_BOUTON_PLUS, HIGH, HIGH, 0};
Bouton boutonMoins = {PIN_BOUTON_MOINS, HIGH, HIGH, 0};

const unsigned long DELAI_ANTI_REBOND = 35;
const unsigned long FENETRE_CLICS_MODE = 1000;
byte nombreClicsMode = 0;
unsigned long finFenetreMode = 0;

bool boutonPresse(Bouton &bouton) {
  bool lecture = chiffrealRead(bouton.pin);

  if (lecture != bouton.lectureBrute) {
    bouton.lectureBrute = lecture;
    bouton.dernierChangement = millis();
  }

  if (millis() - bouton.dernierChangement >= DELAI_ANTI_REBOND &&
      lecture != bouton.etatStable) {
    bouton.etatStable = lecture;
    if (bouton.etatStable == LOW) return true;
  }

  return false;
}

// -------------------- CONTROLE DE LA MONTRE --------------------------
void demarrerMontre() {
  montreActive = true;
  modeReglage = false;
  demarrageActif = true;
  debutDemarrage = millis();
  derniereImageDemarrage = 0;
  imageDemarrage = 0;
  positionContour = 0;
  couleurContour = 0;
  demarrerMelodie();

  ht16k33Initialiser();
  ht16k33Effacer();

  effacerContour();
  ecrireContour();
}

void arreterMontre() {
  montreActive = false;
  demarrageActif = false;
  modeReglage = false;
  nombreClicsMode = 0;
  noTone(PIN_AVERTISSEUR);

  ht16k33Effacer();
  reglerDeuxPoints(false);

  effacerContour();
  ecrireContour();

  // Commande affichage DESACTIVE, oscillateur peut rester actif.
  ht16k33Commande(0x80);
}

void mettreAJourDemarrage() {
  if (!demarrageActif) return;

  unsigned long maintenant = millis();

  if (maintenant - derniereImageDemarrage >= INTERVALLE_IMAGE_DEMARRAGE) {
    derniereImageDemarrage = maintenant;
    afficherEcranDemarrage(imageDemarrage);
    imageDemarrage++;
  }

  if (maintenant - debutDemarrage >= DUREE_DEMARRAGE) {
    demarrageActif = false;
    noTone(PIN_AVERTISSEUR);

    if (!rtcLireHeure()) {
      heureActuelle = 0;
      minuteActuelle = 0;
      secondeActuelle = 0;
    }

    ht16k33AfficherHeure(heureActuelle, minuteActuelle);
    reglerDeuxPoints(true);
  }
}

void reinitialiserHorloge() {
  if (rtcEcrireHeure(0, 0, 0)) {
    heureActuelle = 0;
    minuteActuelle = 0;
    secondeActuelle = 0;
  }

  bipAction();
  ht16k33AfficherHeure(heureActuelle, minuteActuelle);
}

void entrerReglage() {
  if (!rtcLireHeure()) return;

  heureReglage = heureActuelle;
  minuteReglage = minuteActuelle;
  champReglage = REGLAGE_HEURE;
  reglageVisible = true;
  modeReglage = true;
  dernierClignotement = millis();

  ht16k33AfficherReglage();
  bipAction();
}

void validerReglage() {
  if (rtcEcrireHeure(heureReglage, minuteReglage, 0)) {
    heureActuelle = heureReglage;
    minuteActuelle = minuteReglage;
    secondeActuelle = 0;
  }

  modeReglage = false;
  reglageVisible = true;

  ht16k33AfficherHeure(heureActuelle, minuteActuelle);
  reglerDeuxPoints(true);
  bipAction();
}

void traiterClicsMode() {
  if (nombreClicsMode == 0) return;
  if (millis() < finFenetreMode) return;

  if (!modeReglage) {
    if (nombreClicsMode == 1) {
      reinitialiserHorloge();
    } else if (nombreClicsMode == 2) {
      entrerReglage();
    }
  } else {
    if (nombreClicsMode == 1) {
      champReglage =
        (champReglage == REGLAGE_HEURE) ? REGLAGE_MINUTE : REGLAGE_HEURE;

      reglageVisible = true;
      dernierClignotement = millis();
      ht16k33AfficherReglage();
      bipAction();

    } else if (nombreClicsMode == 3) {
      validerReglage();
    }
  }

  nombreClicsMode = 0;
}

void handleBoutons() {
  if (boutonPresse(boutonAlimentation)) {
    if (montreActive) arreterMontre();
    else demarrerMontre();
  }

  if (!montreActive || demarrageActif) return;

  if (boutonPresse(boutonMode)) {
    nombreClicsMode++;
    if (nombreClicsMode > 3) nombreClicsMode = 3;
    finFenetreMode = millis() + FENETRE_CLICS_MODE;
  }

  if (modeReglage) {
    if (boutonPresse(boutonPlus)) {
      if (champReglage == REGLAGE_HEURE) {
        heureReglage = (heureReglage + 1) % 24;
        bipChiffre(heureReglage % 10);
      } else {
        minuteReglage = (minuteReglage + 1) % 60;
        bipChiffre(minuteReglage % 10);
      }

      reglageVisible = true;
      dernierClignotement = millis();
      ht16k33AfficherReglage();
    }

    if (boutonPresse(boutonMoins)) {
      if (champReglage == REGLAGE_HEURE) {
        heureReglage = (heureReglage == 0) ? 23 : heureReglage - 1;
        bipChiffre(heureReglage % 10);
      } else {
        minuteReglage = (minuteReglage == 0) ? 59 : minuteReglage - 1;
        bipChiffre(minuteReglage % 10);
      }

      reglageVisible = true;
      dernierClignotement = millis();
      ht16k33AfficherReglage();
    }
  }

  traiterClicsMode();
}

void mettreAJourClignotement() {
  if (!modeReglage) return;

  if (millis() - dernierClignotement >= INTERVALLE_CLIGNOTEMENT) {
    dernierClignotement = millis();
    reglageVisible = !reglageVisible;
    ht16k33AfficherReglage();
  }
}

// -------------------- CONFIGURATION / BOUCLE ---------------------------
void setup() {
  pinMode(PIN_BOUTON_ALIMENTATION, INPUT_PULLUP);
  pinMode(PIN_BOUTON_MODE, INPUT_PULLUP);
  pinMode(PIN_BOUTON_PLUS, INPUT_PULLUP);
  pinMode(PIN_BOUTON_MOINS, INPUT_PULLUP);
  pinMode(PIN_AVERTISSEUR, OUTPUT);
  pinMode(PIN_DEUX_POINTS, OUTPUT);

  chiffrealWrite(PIN_DEUX_POINTS, LOW);

  Wire.begin();
  Wire.setClock(100000UL);

  // Initialiser le HT16K33 pour que l affichage demarre dans un etat connu.
  ht16k33Initialiser();

  initialiserContour();

  rtcLireHeure();

  montreActive = false;
  demarrageActif = false;
  modeReglage = false;

  ht16k33Effacer();
  ht16k33Commande(0x80); // affichage DESACTIVE
}

void loop() {
  handleBoutons();

  if (!montreActive) return;

  mettreAJourMelodie();
  mettreAJourDemarrage();
  mettreAJourContour();

  if (demarrageActif) return;

  if (!modeReglage && millis() - derniereLectureRTC >= INTERVALLE_LECTURE_RTC) {
    derniereLectureRTC = millis();

    if (rtcLireHeure()) {
      ht16k33AfficherHeure(heureActuelle, minuteActuelle);
    }
  }

  mettreAJourClignotement();
}
