//+------------------------------------------------------------------+
//|                                           WsSimpleClient-1V0.mq4 |
//|                        Copyright 2020, MetaQuotes Software Corp. |
//|                                             https://www.mql5.com |
//+------------------------------------------------------------------+
#property copyright "Copyright 2020, MetaQuotes Software Corp."
#property link      "https://www.mql5.com"
#property version   "1.00"
#property strict

#import "mt-simple-client-websocket.dll"
   int ws_open(uchar &point[]);
   int ws_send(int handle, uchar &message[]);
   int ws_receive(int handle, uchar &buffer[], const int buffer_size);
   void ws_close(int handle);
#import

//--- input parameters
input int      port = 8080;

const int timer_period = 1000;
int handle = 0;

int OnInit() {
   /* инициализируем таймер */
   if(!EventSetMillisecondTimer(timer_period)) return(INIT_FAILED);
   return(INIT_SUCCEEDED);
}

void OnDeinit(const int reason) {
   if(handle != NULL) ws_close(handle);
}

void OnTimer() {
   if(handle != NULL) {
      /*
      string test_str = "test " + Symbol();
      uchar ANSIarray[];
		StringToCharArray(test_str, ANSIarray);
      int ret = ws_send(handle, ANSIarray);
      ArrayFree(ANSIarray);
      if(ret == 0) {
         handle = NULL;
         return;
      }
      Print("WS-Client: Sending message: ", test_str);
      */
      /*
      uchar recv_buffer[];
      ArrayResize(recv_buffer, 65536);
      while(true) {
         int len = ws_receive(handle, recv_buffer, 65536);
         if(len == 0) break;
         string recv_message = CharArrayToString(recv_buffer, 0, len);
         Print("WS-Client: Message received: ", recv_message);
      }
      ArrayFree(recv_buffer);
      */
   } else {
      string url = "localhost:8080/echo";
      uchar ANSIarray[];
		StringToCharArray(url, ANSIarray);
      handle = ws_open(ANSIarray);
      ArrayFree(ANSIarray);
      if(handle != NULL) Print("WS-Client: Opened connection");
      else Print("WS-Client: Failed to establish connection");
   }
}

void OnTick() {

}
