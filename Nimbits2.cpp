#include <Arduino.h>
#include <Dhcp.h>
#include <Dns.h>
#include <EthernetServer.h>
#include <util.h>
#include <EthernetUdp.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include <SPI.h>
#include <stdlib.h>
#include <stdio.h>
#include "Nimbits2.h"
#define BUFFER_SIZE 1024
#define PORT 80
#define CATEGORY 2
#define POINT 1
#define SUBSCRIPTION 5

const char *GOOGLE = "google.com";

const String CLIENT_TYPE_PARAM="&client=arduino";
const String APP_SPOT_DOMAIN = ".appspot.com";
const String PROTOCAL = "HTTP/1.1";
const int WAIT_TIME = 1000;
const char quote = 34;

String _ownerEmail;
String _instance;
String _accessKey;
char _NimText[MaxTextLen];

Nimbits::Nimbits(String instance, String ownerEmail, String accessKey){
  _instance = instance;
  _ownerEmail = ownerEmail;
  _accessKey = accessKey;

}


String createSimpleJason(char *name, char *parent, int entityType) {


}
String floatToString(double number, uint8_t digits) 
{ 
  String resultString = "";
  // Handle negative numbers
  if (number < 0.0)
  {
    resultString += "-";
    number = -number;
  }

  // Round correctly so that print(1.999, 2) prints as "2.00"
  double rounding = 0.5;
  for (uint8_t i=0; i<digits; ++i)
    rounding /= 10.0;

  number += rounding;

  // Extract the integer part of the number and print it
  unsigned long int_part = (unsigned long)number;
  double remainder = number - (double)int_part;
  resultString += int_part;

  // Print the decimal point, but only if there are digits beyond
  if (digits > 0)
    resultString += "."; 

  // Extract digits from the remainder one at a time
  while (digits-- > 0)
  {
    remainder *= 10.0;
    int toPrint = int(remainder);
    resultString += toPrint;
    remainder -= toPrint; 
  } 
  return resultString;
}

// CharToInteger checks that a set of characters represent a valid integer
// and returns that value
//
  int CharToInteger(int startchar, int isize)
  {
    char space = 32;
    char nine = 57;
    char zero = 48;
    char ch;
    int static value;
    boolean iok = true;
    value=0;
        
    for (int i=startchar;i<(startchar+isize);i++){
	ch = _NimText[i];  
        if(ch==space)ch=zero;
	if ((ch<zero)||(ch>nine))  {
	  iok = false;
          value = 9999;
          for (int j=0;j<(4-isize);j++){ value=value/10;}
	  i=isize+startchar+1;
	}
        else {
          value = value*10 + (ch-zero);  // CONVERTS ASCII CHAR VALUE TO INT VALUE
        }
    }
    return value;
  }

// get_line  returns a line of text received from the server.  
// it looks for only the new line character since some servers omit the CR
//

  int get_line(EthernetClient client)
  {
      byte chbyte;
      unsigned long wait_time = 15000;
      unsigned long now, then;
      boolean EOL;
      char ch;
      char cr = 13;  //carriage return
      char nl = 10;  // new line or line feed
      int static len;
      for (len=0;len<80;len++){
        _NimText[len]=0;
      }
      
      len=0;      
      then = millis();
      sprintf(_NimText,"");
      EOL = false;
     
   //   int buf_len = client.available();
   //   Serial.print("get_line: bytes available in ethernet buffer ->");
   //   Serial.println(buf_len);
    
      while (!client.available()){
        now=millis();
        if((now-then)>wait_time){
          Serial.print(" wait time of ->");
          Serial.print(wait_time/1000);
          Serial.println(" seconds exceded in get_line()");
          return len;
        }
      }
  
      do {
        ch = client.read();
        if (ch!=cr) {   //we add it to the test array
          _NimText[len]=ch;
          len+=1;
        }
        if (ch==nl)EOL=true;
      }while ((client.available())&&(!EOL));
    
    return len;
  }
  
// getStatusCode returns the HTTP code from the repsonse header
// Step 1 to find the code, Step 2  get to the end of the header
// if a non-numeric code is not found a 999 will be returned.
// if no code is found a 998 will be returned
// if the end of characters is found before the end of the header a 997 is returned.
// If the code is 100 processing just continues until a 200 (OK) or error code is found
//
  int getStatusCode(EthernetClient client) {
	boolean haveCode;
	boolean EOH = false;  //End Of Header
	int len;
	int code;
        char space = 32;
        char nl = 10;
	byte chbyte;
        int iblank;

 //    	Serial.println("In getStatusCode");
        haveCode = false;
	sprintf(_NimText, "searching for http status code");

	while (!haveCode) {
          
          len=get_line(client);
          
	  if (len == 0) {   // no more data
		code=998;
		haveCode = true;
		EOH = true;
	  }
         
  // the status code should be after the first blank character after the HTTP token
          int index;
          index = strncmp(_NimText,"HTTP",4);
//          Serial.print("index->");
//          Serial.println(index);
          if (index==0){
            	iblank=0;
                for(int i=4;i<len;i++){
                  if (_NimText[i]==space){
                    iblank=i;
                    i=len;
                  }
                }
		if (iblank>0) { 
		      code = CharToInteger((iblank+1),3);
		      Serial.print("HTTP Return Code ->");
		      Serial.println(code);
		      if (code!=100) haveCode = true;
		}
		else{  //  "HTTP" was found but no code so make one up
		      code = 998;
		      haveCode = true;
		}
	  }
        }

	//we have the code so now find the end of the header
	while (!EOH) {
		len=get_line(client);
		if (_NimText[0]==nl){
		    EOH = true;
		}
		else if (len=0) {      //we have a badly formed response
			    code = 997;
			    EOH = true;
		}
	}
        return code;
  }

// getData. This routine will parse the data returned from the get request.
// if bad information is found "99" will be inserted

  int getData(EthernetClient client, int count, struct NimSeries &Nim)
  {
    int len,i;
    int returnCount = 0;
    char ch;
    byte EOL = 0;

//    Serial.println("In getData");

    len=get_line(client);  //this should be the header
    len=min(len,39);  // make sure we do  not write iver the edge of the header

    for (i=0;i<(len-1);i++){
      Nim.DataHeader[i]=_NimText[i];
    }
    Nim.DataHeader[len]=EOL;
 
    for (i=0;i<count;i++){
      len=get_line(client);
  
      if  (len>1) {  // then we have more than the new line char so time to parse
        Nim.Year[returnCount]=CharToInteger(0,4);
        Nim.Month[returnCount]=CharToInteger(5,2);
        Nim.Day[returnCount]=CharToInteger(8,2);
        Nim.Hour[returnCount]=CharToInteger(11,2);
        Nim.Minute[returnCount]=CharToInteger(14,2);
        Nim.Second[returnCount]=CharToInteger(17,2);
        for (int j = 0;j<17;j++){
          ch = _NimText[20+j];
          Nim.Data[returnCount][j]=ch;
          if (ch==EOL) j=17;
        }
       returnCount+=1;
    } else {
        Serial.println("In GetData: end of data before end of count");
        i=count;    // time to go back - no more data
      }        
  }
    
  return returnCount;
}


void Nimbits::recordValue(double value, String note, char *pointId) {
  EthernetClient client;

  String json;
  json =  "{\"d\":\"";
  json +=floatToString(value, 4);

  json += "\",\"n\":\""; 
  json +=  note; 
  json +=  "\"}"; 
  String content;
  content = "email=";

  content += _ownerEmail;
  content += "&key=";
  content += _accessKey;
  content += "&json=";
  content += json;
  content += "&id=";
  content +=  pointId;

  Serial.println(content);

  if (client.connect(GOOGLE, PORT)) {
    client.println("POST /service/v2/value HTTP/1.1");
    client.println("Host:nimbits-02.appspot.com");
    client.println("Connection:close");
    client.println("User-Agent: Arduino/1.0");
    client.println("Cache-Control:max-age=0");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.println(content.length());
    client.println();
    client.println(content);

    while(client.connected() && !client.available()) delay(1);
    while (client.available() ) {
      char c = client.read();
      Serial.print(c);

    }
  }

}

void Nimbits::addEntityIfMissing(char *key, char *name, char *parent, int entityType, char *settings) {
  EthernetClient client;
  Serial.println("adding");
  String retStr;
  char c;
  // String json;
  String json;
  json =  "{\"name\":\"";
  json += name; 
  json.concat("\",\"description\":\""); 
  json +=   "na"; 
  json += "\",\"entityType\":\""; 
  json +=  String(entityType); 
  json +=  "\",\"parent\":\""; 
  json +=   parent; 
  json += "\",\"owner\":\""; 
  json +=  _ownerEmail;
  json +=  String("\",\"protectionLevel\":\"");
  json +=   "2";
  
  //return json;
  switch (entityType) {
  case 1: 
    // json = createSimpleJason(name, parent, entityType); 

    break;
  case 2: 
    // json = createSimpleJason(name, parent, entityType); 
    break;
  case 5: 
    json +=  "\",\"subscribedEntity\":\""; 
    json +=   parent; 
    json +=  "\",\"notifyMethod\":\""; 
    json +=   "0"; 
    json +=  "\",\"subscriptionType\":\""; 
    json +=   "5"; 
    json +=  "\",\"maxRepeat\":\""; 
    json +=   "30"; 
    json +=  "\",\"enabled\":\""; 
    json +=   "true"; 

    break;
  }
  json += settings;
  json +=  "\"}";
  Serial.println(json);
  String content;
  content = "email=";

  content += _ownerEmail;
  content += "&key=";
  content += _accessKey;
  content += "&json=";
  content += json;
  content += "&action=";
  content += "createmissing";
  Serial.println(content);
  if (client.connect(GOOGLE, PORT)) {
    client.println("POST /service/v2/entity HTTP/1.1");
    client.println("Host:nimbits-02.appspot.com");
    client.println("Connection:close");
    client.println("User-Agent: Arduino/1.0");
    client.println("Cache-Control:max-age=0");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.println(content.length());
    client.println();
    client.println(content);

    while(client.connected() && !client.available()) delay(1);
    int contentLength = 0;
    char buffer[BUFFER_SIZE];


    while (client.available() && contentLength++ < BUFFER_SIZE) {
      c = client.read();
      Serial.print(c);
      buffer[contentLength] = c;
    }
    Serial.println("getKeyFromJson");
    Serial.println(sizeof(buffer));
    int i=0;
    char item[] = {
      "\"key\":"          };
    while (i++ < sizeof(buffer) - sizeof(item)) {
      boolean found = false;
      found = false;
      for (int v = 0; v < sizeof(item) -1; v++) {

        if (buffer[i+v] != item[v]) { 
          found = false;
          break;
        }
        found = true;
      }
      if (found) {
        break;
      }


    }

    i = i + sizeof(item)-1;
    int keySize = 0;
    while (i++ < sizeof(buffer)-1) {
      if (buffer[i] == quote) {
        break;
      }
      else {
        key[keySize++] = buffer[i];
      }
    }
    key[keySize] = '\0';
    Serial.println(key);



    client.stop();
  }
  else {
    Serial.println("could not connect");

  }
  delay(1000);
}


int Nimbits::getSeries(int count, char *pointId, struct NimSeries &Nim) 
{
  EthernetClient client;
  
  int static series_count;  // returns the number of elements found or an error code
  int sindex;
  int http_status_code;
  char status_text[40];
  int getcount;
  char charCount[3];
  String get_string;
  Serial.println("In getSeries");
  
  getcount=min(count,ArraySize);
  sprintf(charCount,"%02d",getcount);

  get_string = "GET /service/v2/series?&format=csv&count=";
  get_string += charCount;
  get_string += "&email=";
  get_string += _ownerEmail; 
  get_string += "&key=";
  get_string += _accessKey;
  get_string += "&id=";
  get_string +=  pointId;

//    Serial.print(get_string);
//    Serial.println(" HTTP/1.1");

  if (client.connect(GOOGLE, PORT)) {
    Serial.println("client connect OK");
    

    client.print(get_string);
    client.println(" HTTP/1.1");

    client.println("Host:nimbits-02.appspot.com");
    client.println("Connection:close");
    client.println("User-Agent: Arduino/1.0");
    client.println("Cache-Control:max-age=0");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println();
    Serial.println("Get request sent");
  } 
  else{
    http_status_code = 996;
    sprintf(status_text,"client.connect failed");
    series_count = -http_status_code;
    Serial.print("Error in client.connect");
    return series_count;
  }

    while(client.connected() && !client.available()) delay(1);  // wait for the response / add time out code

    http_status_code = getStatusCode(client);
    
//    Serial.print(http_status_code);

    if (http_status_code<300) {
	    series_count = getData (client, count, Nim);
    }
    else{
	    series_count = -http_status_code;
            Serial.print(_NimText);
    }
    
    while (client.available() ) {
      char c = client.read();
    //  Serial.print(c);

    }
  
  return series_count;
}


//String Nimbits::getTime() {
//  EthernetClient client;

// if (client.connect(GOOGLE, PORT)) {
//  client.print("GET /service/v2/time?");
//writeAuthParamsToClient(client);
//writeHostToClient(client);

// return getResponse(client);


//}




//record a value

//record data

//create point

//create point with parent

//batch update

//delete point

//get children with values





























