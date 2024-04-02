#include <DS3231.h> //Librairie utile au module RTC
#include <SoftwareSerial.h> //Librairie permettant de simuler une communication série sur deux broches de l'Arduino

SoftwareSerial Bluetooth(4,5); //RX = Pin 4; TX = Pin 5;
String msg;//Variable contenant le message reçu du téléphone

DS3231  Horloge(SDA, SCL);//On déclare l'horloge
Time  T;//Variable temporelle (contenant heure, min, jour de la semaine, mois, année etc...)
unsigned long MomentDAllumage; //Variable contenant la date d'allumage (en s)
unsigned long MomentDExtinction;//Idem pour l'extinction
int RequeteDAllumageHeure;//Heure d'allumage demandée
int RequeteDAllumageMin;
int RequeteDExtinctionHeure;
int RequeteDExtinctionMin;
bool Avertissement = false;//Variable indicant la nécessité ou non d'avertir l'utilisateur
bool etat=false;//État du relai

const int relai = 9;//Pin du relai
const int buzzer = 10;//Pin du buzzer

int Amperemetre = A6;//Pin de l'ampèremètre
double moyenneLocale = 0;//Première moyenne permettant de faire la moyenne de 100 valeurs mesurées
double sensibilite = 185.0; //Sensibilité du module correspondant à 185mV/A
float voltage =0.0; // Tension mesurée aux bornes du capteur (en mV)
float I = 0.0;//Intensité mesurée
float Itot;//Somme des intensités
float Q;//Quantité d'électricité en Ampère-Secondes
int NMesure=0;//Nombre de mesures effectuées
int Duree;//Durée d'ouverture du relai en S
float E;//Énergie mesurée
float U=5.00;//Tension du d'alimentation du circuit mesuré

void CalculMomentDAllumage(){   
  MomentDAllumage = T.min*60+T.sec;
}
void CalculMomentDExtinction(){
  MomentDExtinction = T.min*60+T.sec;
}

void MesureEnergie()
{
  moyenneLocale = 0;
  for(int i = 0; i < 100; i++) {   
  moyenneLocale += analogRead(Amperemetre); //On effectue la mesure de 100 valeurs mesurée
  delay(1);
  }
  moyenneLocale /= 100; //On effectue la moyenne pour obtenir la valeur de voltage la plus précise
  voltage = (moyenneLocale / 1023.0) * 5000.0; // On détermine la valeur de voltage en mV
  I = (voltage - 2521) / sensibilite - 0.12; //On détermine une valeur de I
  if(I>=0.1) //Si elle est négative ou nulle, on ne la prend pas en compte
  {
    NMesure++;
    Itot+=I;
  } 

  if(etat)Duree = (T.min*60+T.sec) - MomentDAllumage; //Tant que le relai est fermé, la durée correspond à la différence entre la date de la mesure et le moment d'allumage
  else if (!etat) Duree = MomentDExtinction - MomentDAllumage;//Une fois qu'il est ouvert, il vaudra la différence entre la date d'extinction et la date d'allumage
  
  Q=(Itot/NMesure)*Duree; // On applique la formule afin de trouver la quantité d'électricité
  E=U*Q;//On calcule l'énergie
  Serial.print("Intensité(A): ");
  Serial.print(I);
  Serial.print(" Durée(s) : ");
  Serial.print(Duree);
  Serial.print(" Énergie(Ws) : ");
  Serial.println(E);
  
}

void setup() {
  Serial.begin(9600);//Initialisation du moniteur série
  Bluetooth.begin(9600);//Initialisation de la communicaion avec le Bluetooth
  Horloge.begin();//Initialisation du module RTC
  pinMode(relai, OUTPUT);//Le pin 9 en sortie
  pinMode(buzzer, OUTPUT);//Idem pour le pin 10
  digitalWrite(relai, LOW);//On ouvre le relai

}

void loop() {
  
  T = Horloge.getTime(); //On inclut la date dans la variable T;
  if (Bluetooth.available()) {
    msg = Bluetooth.readString(); //Si un message et reçu, il est stocké dans msg
    Serial.println(msg);
  
    if(msg.startsWith("On")){ //L'ordre d'allumage instantanée...
      digitalWrite(relai, HIGH);//... ferme le relai
      CalculMomentDAllumage(); 
      etat = true;
      }
    
    if(msg.startsWith("Off")){//L'ordre d'extinction instantanée...
      digitalWrite(relai, LOW);//... ouvre le relai
      CalculMomentDExtinction();
      etat=false;}

    if(msg.startsWith("A"))//Si le message commence par "A", alors il s'agit d'un horaire d'allumage de la forme AXXXX
    {
      msg = msg.substring(1, msg.length());//On retire le A de msg
      RequeteDAllumageHeure = msg.toInt()/100;//On convertit msg en nombre entier et on le divise par sonn pour obtenir son ombre de dizainnes : c'est l'heure
      RequeteDAllumageMin = msg.toInt() % 100;//Conversion de msg en nombre entier et on met dans la variable le reste de la division euclidienne de msg par 100 : ce sont les minutes 
      MomentDAllumage= (msg.toInt()%100)*60;//On convertit le moment d'allumage en secondes
    }
    if(msg.startsWith("E"))//La logique est ici la même que pour la requête d'allumage 
    {
      msg = msg.substring(1, msg.length());
      RequeteDExtinctionHeure = msg.toInt()/100;
      RequeteDExtinctionMin = msg.toInt() % 100;
      Avertissement = false;
      MomentDExtinction=(msg.toInt()%100)*60;
    }
    
    if(msg.startsWith("Conso"))//Si la demande de consommation est reçu...
    {
      Bluetooth.println(E);//...On envoie la valeur de E
    }

  }

  if(T.hour == RequeteDAllumageHeure && T.min == RequeteDAllumageMin)//Si l'heure et les minutes correspondent à la requête d'allumage...
    {
      digitalWrite(relai, HIGH);//...on ferme le relai
      etat=true;
    }
    
  if(T.hour == RequeteDExtinctionHeure && T.min == RequeteDExtinctionMin-5 && !Avertissement)//Si on est 5 minutes avant l'horaire d'extinction et que l'avertissement n'a pas eu lieu...
    {
      for(int i=0; i<= 1; i++)//...On enclenche un avertissement sonore
      {
        for(int j=0; j<=2; j++)
        {
         digitalWrite(buzzer,HIGH);
         delay(100);
         digitalWrite(buzzer,LOW);
         delay(100);
        }
        delay(500);
      }
      Avertissement = true;//L'avertissement a bien eu lieu
    }

   if(T.hour == RequeteDExtinctionHeure && T.min == RequeteDExtinctionMin)//Si l'heure et les minutes correspondent à la requête d'allumage...
    {
      digitalWrite(relai, LOW);//...on ferme le relai
      Bluetooth.println(E);
      etat=false;
    }
    
    MesureEnergie();//On effectue la mesure de l'énergie consommée par le dipôle
    
   delay(1000);

}
