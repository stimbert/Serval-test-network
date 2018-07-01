#include <stdio.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <time.h>
#include "sync.h"
#include "lbard.h"
#include "hf.h"
#include "radios.h"

radio_type radio_types[]={
  {RADIOTYPE_HFBARRETT,"hfbarrett","Barrett HF with ALE",hfcodanbarrett_radio_detect,hfbarrett_serviceloop,hfbarrett_receive_bytes,hfbarrett_send_packet,hf_radio_check_if_ready,20},
  {RADIOTYPE_HFCODAN,"hfcodan","Codan HF with ALE",hfcodanbarrett_radio_detect,hfcodan_serviceloop,hfcodan_receive_bytes,hfcodan_send_packet,hf_radio_check_if_ready,10},
  {RADIOTYPE_RFD900,"rfd900","RFDesign RFD900, RFD868 or compatible",rfd900_radio_detect,rfd900_serviceloop,rfd900_receive_bytes,rfd900_send_packet,always_ready,0},
  {-1,NULL,NULL,NULL,NULL,NULL,NULL,NULL,-1}
};
