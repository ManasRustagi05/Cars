  
 int val = 1 ;  
 void setup()  
 {  
 Serial.begin(9600); // sensor buart rate  
 pinMode(6,INPUT);  // IR sensor output pin connected  
 pinMode(2,OUTPUT);   
 pinMode(3,OUTPUT);   
 pinMode(4,OUTPUT);    
 pinMode(5,OUTPUT);   
 pinMode(7,OUTPUT);  // BUZZER  
 }  
 void loop()  
 {  
 val = digitalRead(6);  //  IR sensor output pin connected  
 Serial.println(val);  // see the value in serial monitor in Arduino IDE  
 delay(500);  
 if(val == 0 )  
 { 
  digitalWrite(2, HIGH);
  digitalWrite(3, LOW);
  digitalWrite(4, HIGH);
  digitalWrite(5, LOW);   
  digitalWrite(7,HIGH);  // BUZZER ON  
 }  
 else  
 {  
 digitalWrite(2, LOW);
  digitalWrite(3, LOW);
  digitalWrite(4, LOW);
  digitalWrite(5, LOW);  
  digitalWrite(7,LOW);  // BUZZER ON 
 }  
 }
