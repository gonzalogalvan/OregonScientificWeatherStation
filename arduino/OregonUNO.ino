#include <SPI.h>
#include <EtherCard.h>                               

#define REQUEST_RATE 5000  // Frecuencia de envío al servidor
#define READ_RATE 10000  // Frecuencia de envío al servidor
#define BLINK_RATE 1000
#define ETHERCARD_CS 10
#define UIPETHERNET_DEBUG_CLIENT 1

// Dirección mac de la interfaz
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x36 };
// Dirección del servidor web
//char const website[] PROGMEM = "data.sparkfun.com";
char const website[] PROGMEM = "data.sparkfun.com";
byte Ethernet::buffer[500]; 

char tempChar[10], postData[100];

boolean pendienteEnvio = false;
static long timer,timerHttp, timerRead,timerBlink;

class DecodeOOK {
protected:
    byte total_bits, bits, flip, state, pos, data[25];
 
    virtual char decode (word width) =0;
 
public:
 
    enum { UNKNOWN, T0, T1, T2, T3, OK, DONE };
 
    DecodeOOK () { resetDecoder(); }
 
    bool nextPulse (word width) {
        if (state != DONE)
 
            switch (decode(width)) {
                case -1: resetDecoder(); break;
                case 1:  done(); break;
            }
        return isDone();
    }
 
    bool isDone () const { return state == DONE; }
 
    const byte* getData (byte& count) const {
        count = pos;
        return data; 
    }
 
    void resetDecoder () {
        total_bits = bits = pos = flip = 0;
        state = UNKNOWN;
    }
 
    // add one bit to the packet data buffer
 
    virtual void gotBit (char value) {
        total_bits++;
        byte *ptr = data + pos;
        *ptr = (*ptr >> 1) | (value << 7);
 
        if (++bits >= 8) {
            bits = 0;
            if (++pos >= sizeof data) {
                resetDecoder();
                return;
            }
        }
        state = OK;
    }
 
    // store a bit using Manchester encoding
    void manchester (char value) {
        flip ^= value; // manchester code, long pulse flips the bit
        gotBit(flip);
    }
 
    // move bits to the front so that all the bits are aligned to the end
    void alignTail (byte max =0) {
        // align bits
        if (bits != 0) {
            data[pos] >>= 8 - bits;
            for (byte i = 0; i < pos; ++i)
                data[i] = (data[i] >> bits) | (data[i+1] << (8 - bits));
            bits = 0;
        }
        // optionally shift bytes down if there are too many of 'em
        if (max > 0 && pos > max) {
            byte n = pos - max;
            pos = max;
            for (byte i = 0; i < pos; ++i)
                data[i] = data[i+n];
        }
    }
 
    void reverseBits () {
        for (byte i = 0; i < pos; ++i) {
            byte b = data[i];
            for (byte j = 0; j < 8; ++j) {
                data[i] = (data[i] << 1) | (b & 1);
                b >>= 1;
            }
        }
    }
 
    void reverseNibbles () {
        for (byte i = 0; i < pos; ++i)
            data[i] = (data[i] << 4) | (data[i] >> 4);
    }
 
    void done () {
        while (bits)
            gotBit(0); // padding
        state = DONE;
    }
};
 
class OregonDecoderV2 : public DecodeOOK {
  public:   
 
    OregonDecoderV2() {}
 
    // add one bit to the packet data buffer
    virtual void gotBit (char value) {
        if(!(total_bits & 0x01))
        {
            data[pos] = (data[pos] >> 1) | (value ? 0x80 : 00);
        }
        total_bits++;
        pos = total_bits >> 4;
        if (pos >= sizeof data) {
            Serial.println("sizeof data");
            resetDecoder();
            return;
        }
        state = OK;
    }
 
    virtual char decode (word width) {
       if (200 <= width && width < 1200) {
            //Serial.println(width);
            byte w = width >= 700;
 
            switch (state) {
                case UNKNOWN:
                    if (w != 0) {
                        // Long pulse
                        ++flip;
                    } else if (w == 0 && 24 <= flip) {
                        // Short pulse, start bit
                        flip = 0;
                        state = T0;
                    } else {
                        // Reset decoder
                        return -1;
                    }
                    break;
                case OK:
                    if (w == 0) {
                        // Short pulse
                        state = T0;
                    } else {
                        // Long pulse
                        manchester(1);
                    }
                    break;
                case T0:
                    if (w == 0) {
                      // Second short pulse
                        manchester(0);
                    } else {
                        // Reset decoder
                        return -1;
                    }
                    break;
              }
        } else if (width >= 2500  && pos >= 8) {
            return 1;
        } else {
            return -1;
        }
        return 0;
    }
};
//===================================================================
class OregonDecoderV3 : public DecodeOOK {
  public:   
 
    OregonDecoderV3() {}
 
    // add one bit to the packet data buffer
    virtual void gotBit (char value) {
        data[pos] = (data[pos] >> 1) | (value ? 0x80 : 00);
        total_bits++;
        pos = total_bits >> 3;
        if (pos >= sizeof data) {
            //Serial.println("sizeof data");
            resetDecoder();
            return;
        }
        state = OK;
    }
 
    virtual char decode (word width) {
       if (200 <= width && width < 1200) {
            //Serial.println(width);
            byte w = width >= 700;
 
            switch (state) {
                case UNKNOWN:
                    if (w == 0) {
                        // Long pulse
                        ++flip;
                    } else if (32 <= flip) {
                        flip = 1;
                        manchester(1);
                    } else {
                        // Reset decoder
                        return -1;
                    }
                    break;
                case OK:
                    if (w == 0) {
                        // Short pulse
                        state = T0;
                    } else {
                        // Long pulse
                        manchester(1);
                    }
                    break;
                case T0:
                    if (w == 0) {
                      // Second short pulse
                        manchester(0);
                    } else {
                        // Reset decoder
                        return -1;
                    }
                    break;
              }
        } else  {
            // Trame intermédiaire 48bits ex: [OSV3 6281 3C 6801 70]
            return  (total_bits <104 && total_bits>=40  ) ? 1: -1;
        }
        
        return (total_bits == 104) ? 1: 0;
    }
};

//===================================================================

OregonDecoderV2 orscV2;
OregonDecoderV3 orscV3;
 
volatile word pulse;
 
void ext_int_1(void)
{
    static word last;
    // determine the pulse length in microseconds, for either polarity
    pulse = micros() - last;
    last += pulse;
}
 
void reportSerial (const char* s, class DecodeOOK& decoder) {
    byte pos;
    const byte* data = decoder.getData(pos);
    Serial.print(s);
    Serial.print(' ');
    for (byte i = 0; i < pos; ++i)
    {
        Serial.print(data[i] >> 4, HEX);
        Serial.print(data[i] & 0x0F, HEX);
    }
 
    Serial.println();

     // Energy OWL : CMR180
    if(data[2] == 0x3C )
    {
       Serial.print(F("[CMR180,...] Id:"));
       Serial.println(data[0], HEX);Serial.print(data[1], HEX);
       Serial.println(pos);
       Serial.println(data[3] & 0x0F, HEX);
       Serial.println(power(data)); 
       if (pos > 6) {

         Serial.println(total(data));
         Serial.println(total(data)/3600/1000);
       }
      
    }
 
 
    // Outside/Water Temp : THN132N,...
    if(data[0] == 0xEA && data[1] == 0x4C)
    {
       Serial.print(F("[THN132N,...] Id:"));
       Serial.println(data[3], HEX);
       Serial.println(channel(data));
       Serial.println(temperature(data));
       Serial.println(battery(data)); 

       float temperatureRec =temperature(data);
       byte channelRec = channel(data);
       byte batteryRec = battery(data);
    
        String(temperatureRec,1).toCharArray(tempChar,10);
        sprintf(postData, "type=%02X&id=%02X&channel=%02X&temp=%s&bat=%d",data[0],data[3],channelRec,tempChar,batteryRec);

      
    }
    // Inside Temp-Hygro : THGR228N,...
    else if(data[0] == 0x1A && data[1] == 0x2D)
    {
       Serial.print(F("[THGR228N,...] Id:"));
       Serial.println(data[3], HEX);
       Serial.println(channel(data));
       Serial.println(temperature(data));
       Serial.println(humidity(data));
       Serial.println(battery(data)); 
      
       float temperatureRec =temperature(data);
       byte channelRec = channel(data);
       byte humidityRec = humidity(data);
       byte batteryRec = battery(data);
    

       String(temperatureRec,1).toCharArray(tempChar,10);

       sprintf(postData, "type=%02X&id=%02X&channel=%02X&temp=%s&humidity=%d&battery=%d",data[0],data[3],channelRec,tempChar,humidityRec,batteryRec);



      if(millis() > timerRead + READ_RATE) {
          timerRead = millis();
          pendienteEnvio = true;
      }
    }
    
    decoder.resetDecoder();
}

unsigned long total(const byte* d){
  long val = 0;
  val = (unsigned long)d[8]<<24;
//  Serial.println();
//  Serial.print(" val:"); Serial.print(val,HEX); Serial.print(" ");
//  Serial.println(d[8], HEX);
  val += (unsigned long)d[7] << 16;
//  Serial.print(" val:"); Serial.print(val,HEX); Serial.print(" ");
//  Serial.println(d[7], HEX);
  val += d[6] << 8;
//  Serial.print(" val:"); Serial.print(val,HEX); Serial.print(" ");
//  Serial.println(d[6], HEX);
  val += d[5];
//  Serial.print(" val:"); Serial.print(val,HEX); Serial.print(" ");
//  Serial.println(d[5], HEX);
  return val ;
}
 float temperature(const byte* data)
{
    int sign = (data[6]&0x8) ? -1 : 1;
    float temp = ((data[5]&0xF0) >> 4)*10 + (data[5]&0xF) + (float)(((data[4]&0xF0) >> 4) / 10.0);
    return sign * temp;
}
 
byte humidity(const byte* data)
{
    return (data[7]&0xF) * 10 + ((data[6]&0xF0) >> 4);
}
 
// Ne retourne qu'un apercu de l'etat de la batterie : 10 = faible
byte battery(const byte* data)
{
    return (data[4] & 0x4) ? 10 : 90;
}
 
byte channel(const byte* data)
{
    byte channel;
    switch (data[2])
    {
        case 0x10:
            channel = 1;
            break;
        case 0x20:
            channel = 2;
            break;
        case 0x40:
            channel = 3;
            break;
     }
 
     return channel;
}

unsigned int power(const byte* d){
  unsigned int val = 0;
  val += d[4] << 8;
  val += d[3];
  return val & 0xFFF0 ;
}


void setup ()
{
    pinMode(7, OUTPUT);
    digitalWrite(7, HIGH);
    
  //Configuramos las consola 
  Serial.begin(9600);
  Serial.println(F("\n[ookDecoder]"));
  
  //Confiramos las interripciones en el pin 21 del Mega
  attachInterrupt(0, ext_int_1, CHANGE);

  //Inicio configuración interfaz ethernet.


  Serial.println("Init Ethercard...");
  if (ether.begin(sizeof Ethernet::buffer, mymac,ETHERCARD_CS) == 0) {
      Serial.println(F("Problemas accediendo a la interfaz !!"));
      while(1);
  }
  Serial.println(F("Configurando por DHCP ..."));
  if (!ether.dhcpSetup()) {
      Serial.println(F("Fallo en DHCP !!"));
      while(1);
  }

  //Log de la configuración
  ether.printIp("Mi IP: ", ether.myip);
  ether.printIp("Puerta de Enlace IP: ", ether.gwip);
  ether.printIp("DNS IP: ", ether.dnsip);
    
  
  //Resolvemos el nombre del servidor para obtener la IP
  if (!ether.dnsLookup(website)) {
      Serial.println(F("Fallo en DNS!!"));
      while(1);
  }
  
  //Inicializacion de timers
  timerHttp = -REQUEST_RATE;
  timerRead = -READ_RATE;


 // pinMode(13, OUTPUT);
         
  //Fin inicalización
  Serial.println(F("Esperando ....           "));

  digitalWrite(7, LOW);
}
 
void loop () {
    static int i = 0;
    cli();
    word p = pulse;
 
    pulse = 0;
    sei();
 
    if (p != 0)
    {
          if (orscV2.nextPulse(p))
            reportSerial("OSV2", orscV2);  
        if (orscV3.nextPulse(p))
             reportSerial("OSV3", orscV3);
    }
   
     
   if(pendienteEnvio) {
      
 
     //Enviamos al servidor
      if(millis() > timerHttp + REQUEST_RATE) {

       Serial.println(F(">> SEND"));
       Serial.println(postData);

       ether.browseUrl(PSTR("/input/EJGr437o50fnEbalobwJ?private_key=dqWYn1xwejTbJG57RGvM&"),postData , website, procesarRespuesta);

       
       timerHttp = millis();
       
      }

     ether.packetLoop(ether.packetReceive());  
   }
   blinkLed(1);
}

static void procesarRespuesta (byte status, word off, word len) {
   Serial.println(F("<< OK"));
  pendienteEnvio = false;
} 

void blinkLed(int pulso) {
   if(millis() > timerBlink + BLINK_RATE*2*pulso) {
     timerBlink = millis();
   }
    
   if(millis() > timerBlink + BLINK_RATE*pulso) {
        digitalWrite(7, LOW);    // turn the LED off by making the voltage LOW
   } else {
       digitalWrite(7, HIGH);   // turn the LED on (HIGH is the voltage level)
   }

}


