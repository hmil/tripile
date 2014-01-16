#include <robopoly.h>
#include <Servo.h>

#define LOW_BATT_VAL            720

#define SONDE_RETRACTED         67
#define SONDE_DEPLOYED_BIG      140
#define SONDE_DEPLOYED_LITTLE   170

#define DISTRIB_MIDDLE          78
#define DISTRIB_LOW             5
#define DISTRIB_HIGH            180

#define DISTRIBUTION_DELAY      700
#define SONDE_DEPLOYMENT_DELAY  500
#define NOISE_THRESHOLD         50
#define PILE_MINIMAL_TENSION    220

//En dessous de cette valeur, l'objet testé est considéré comme un accu
#define ACCU_THRESHOLD          270
/*  Ces valeurs sont comparées aux tensions mesurées en décharge
  hypothèses : 
  - accu : U = 1.2V, r ~= 0.02ohm
  - pile : U = 1.5V, r ~= 1ohm
  
  avec une certaine marge de manoeuvre car ces résultats théoriques 
  peuvent facilement faire passer une pile chargée pour déchargée
  */
#define PILE_LOADED             100
#define ACCU_LOADED             90


//Macro permettant de finaliser un test  
#define FINISH_TEST(success)       \
  servoOneShot(sonde, SONDE_RETRACTED, SONDE_DEPLOYMENT_DELAY);    \
  *result = success;               \
  return true; 

int pilePin = 0;
int batteryPin = 1;

int cptPin = 15;

unsigned char sonde = 5;

unsigned char distrib = 7;

int triggerPin = 13;

int buttonPin = 30;
bool buttonDown = false;

int lowBattLed = 26;
int okLed = 28;
int nokLed = 27;

void checkBattery(){
  int level = analogRead(batteryPin);
  if(level < LOW_BATT_VAL){
    digitalWrite(lowBattLed, true);
  }
}

//Deplace le servo a la position voulue puis le déconnecte (fonction bloquante)
void servoOneShot(unsigned char id, unsigned char angle, int time){
  setServo(id, angle);
  delay(time);
  unsetServo(id);
}

void setup(){
  Serial.begin(9600);
  pinMode(pilePin, INPUT);
  pinMode(batteryPin, INPUT);
  pinMode(triggerPin, OUTPUT);
  pinMode(buttonPin, INPUT);
  pinMode(cptPin, INPUT);
  digitalWrite(triggerPin, LOW);
  
  pinMode(okLed, OUTPUT);
  pinMode(nokLed, OUTPUT);
  pinMode(lowBattLed, OUTPUT);
  
  digitalWrite(okLed, LOW);
  digitalWrite(nokLed, LOW);
  digitalWrite(lowBattLed, LOW);

  servoOneShot(sonde, SONDE_RETRACTED, SONDE_DEPLOYMENT_DELAY);

  servoOneShot(distrib, DISTRIB_MIDDLE, DISTRIBUTION_DELAY);
  
  //On attend que les servo aient fini de tourner pour faire le test de batterie
  delay(200);
  checkBattery();
}

void onPileOk(){
  Serial.println("Pile chargée");
  digitalWrite(okLed, HIGH);
  servoOneShot(distrib, DISTRIB_HIGH, DISTRIBUTION_DELAY);
  servoOneShot(distrib, DISTRIB_MIDDLE, DISTRIBUTION_DELAY);
  digitalWrite(okLed, LOW);
}

void onPileNok(){
  Serial.println("Pile déchargée");
  digitalWrite(nokLed, HIGH);
  servoOneShot(distrib, DISTRIB_LOW, DISTRIBUTION_DELAY);
  servoOneShot(distrib, DISTRIB_MIDDLE, DISTRIBUTION_DELAY);
  digitalWrite(nokLed, LOW);
}

bool makeTest(int sondeValue, bool *result){
  int initialValue = 0,
      chargeValue = 0;
  
  servoOneShot(sonde, sondeValue, SONDE_DEPLOYMENT_DELAY);
  
  initialValue = analogRead(pilePin);
  
  
  Serial.print("Premiere valeur : ");
  //Serial.println(initialValue);
  //Si on ne capte rien, le test est un échec
  if(initialValue < NOISE_THRESHOLD){
    Serial.println("Valeur trop basse.");

    return false;
  }
  
  //ON a détecté quelque chose mais c'est trop faible
  if(initialValue < PILE_MINIMAL_TENSION){
    Serial.println("Pile déchargée détectée");
    FINISH_TEST(false);
  }
  else{
    
    bool isAccu = (initialValue < ACCU_THRESHOLD);
    //On a une mesure valide, on effectue le test de charge
    digitalWrite(triggerPin, true);
    delayMicroseconds(20);
    chargeValue = analogRead(pilePin);
    digitalWrite(triggerPin, false);
    
    Serial.print("Pile détectée,\ntension en décharge :");
    //Serial.println(chargeValue);
    
    //La tension en décharge est trop faible, test négatif
    if(isAccu && (chargeValue < ACCU_LOADED) || !isAccu && (chargeValue < PILE_LOADED)){
      Serial.println("pile déchargée");
      FINISH_TEST(false);
    }
    //Sinon, le test est positif
    else{
      Serial.println("Pile chargée");
      FINISH_TEST(true);
    }
  }
  
  //Il est impossible d'arriver ici mais au cas ou...
  servoOneShot(sonde, SONDE_RETRACTED, SONDE_DEPLOYMENT_DELAY);
  return false;
}

bool test(){
  bool testResult;
  Serial.println("--- Début des tests ---");
  if(makeTest(SONDE_DEPLOYED_BIG, &testResult)){
    return testResult;
  }
  else if(makeTest(SONDE_DEPLOYED_LITTLE, &testResult)){
    return testResult;
  }
  servoOneShot(sonde, SONDE_RETRACTED, SONDE_DEPLOYMENT_DELAY);
  return false;
}

void start(){
  do{
    if(test()){
      onPileOk();
    }
    else{
      onPileNok();
    }
    //On pousse la pile vers le capteur
    servoOneShot(sonde, SONDE_DEPLOYED_BIG, SONDE_DEPLOYMENT_DELAY);
    
    buttonDown = digitalRead(buttonPin);
  }while(digitalRead(cptPin) && !buttonDown);
  
  servoOneShot(sonde, SONDE_RETRACTED, SONDE_DEPLOYMENT_DELAY);
}

void loop(){
  
  bool buttonState = digitalRead(buttonPin);
  if(digitalRead(buttonPin) && !buttonDown){
    start();
  }
  else if(!buttonState){
    //Evite de relancer un cycle quand l'utilsateur a annulé
    buttonDown = false;
  }
  
  delay(10);
}

