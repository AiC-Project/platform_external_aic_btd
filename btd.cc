#define LOG_TAG "local_bt"
#include <cutils/properties.h>
#include <cutils/log.h>

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#define NO_PROTOBUF
#include "aic.h"

#include <sys/types.h>

#define BT_UPDATE_PERIOD 1

#define SIM_BT_PORT 22700

// Protobuff
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <bt.pb.h>

#include "android_runtime/AndroidRuntime.h"
#include "android_runtime/Log.h"


#include <string.h>


#define KEY_PREFIX "aicd"
#define PROP_BTD KEY_PREFIX".btd"

#define PROP_ACT PROP_BTD".action" //"cmd:name@addr#devclass"

#define PROP_CMD PROP_BTD".cmd"
#define PROP_ADR PROP_BTD".addr"
#define PROP_NAM PROP_BTD".name"
#define PROP_DCL PROP_BTD".devclass"

/*****************************************************************************
**   Logger API
*****************************************************************************/
#define BTDLOG(param, ...) {SLOGD("%s-"param, __FUNCTION__, ## __VA_ARGS__);}

using namespace google::protobuf::io;
using namespace android;

/**
 * ENCODE BT
 **/
#define HCI_LE_RECEIVER_TEST_OPCODE 0x201D
#define HCI_LE_TRANSMITTER_TEST_OPCODE 0x201E
#define HCI_LE_END_TEST_OPCODE 0x201F

#define MASK_BTIF 0b10000000 // 0b 1000 0000
#define MASK_BTE  0b01000000 // 0b 0100 0000




#define isBTIF(vv) (( ( (vv&MASK_BTIF) >> 7)  && !( (vv&MASK_BTE )  >> 6) ) )
#define isBTE(ww)  (( ( (ww&MASK_BTE ) >> 6)  && !( (ww&MASK_BTIF)  >> 7) ) )

#define getBTIF(yy) ( yy & (~(MASK_BTE|MASK_BTIF)) )
#define getBTE(yy)  ( yy & (~(MASK_BTE|MASK_BTIF)) )

enum
{
/*00*/    INIT,
/*01*/    ENABLE,
/*02*/    DISABLE,
/*03*/    CLEANUP,
/*04*/    GET_ADAPTER_PROPERTIES,
/*05*/    GET_ADAPTER_PROPERTY,
/*06*/    SET_ADAPTER_PROPERTY, // SET_ADAPTER_PROPERTY =  SET_ADAPTER_PROPERTY_BDNAME,
/*07*/    GET_REMOTE_DEVICE_PROPERTIES,
/*08*/    GET_REMOTE_DEVICE_PROPERTY,
/*09*/    SET_REMOTE_DEVICE_PROPERTY,
/*10*/    GET_REMOTE_SERVICE_RECORD,
/*11*/    GET_REMOTE_SERVICES,
/*12*/    START_DISCOVERY,
/*13*/    CANCEL_DISCOVERY,
/*14*/    CREATE_BOND,
/*15*/    REMOVE_BOND,
/*16*/    CANCEL_BOND,
/*17*/    PIN_REPLY,
/*18*/    SSP_REPLY,
/*19*/    GET_PROFILE_INTERFACE,
/*20*/    DUT_MODE_CONFIGURE,
/*21*/    DUT_MODE_SEND,
/*22*/    LE_TEST_MODE,
/*23*/    CONFIG_HCI_SNOOP_LOG,
//... ... ...//
/*24*/    SET_ADAPTER_PROPERTY_BDNAME,
/*25*/    SET_ADAPTER_PROPERTY_SCAN_MODE_NONE,
/*26*/    SET_ADAPTER_PROPERTY_SCAN_MODE_CONNECTABLE,
/*27*/    SET_ADAPTER_PROPERTY_SCAN_MODE_CONNECTABLE_DISCOVERABLE,
/*28*/    SET_ADAPTER_PROPERTY_DISCOVERY_TIMEOUT_2M,
/*29*/    SET_ADAPTER_PROPERTY_DISCOVERY_TIMEOUT_5M,
/*30*/    SET_ADAPTER_PROPERTY_DISCOVERY_TIMEOUT_1H,
/*31*/    SET_ADAPTER_PROPERTY_DISCOVERY_TIMEOUT_NE
};

enum
{
    INQ_RES,
    BOND_STATE,
    DISC_RES
};

uint8_t setBTIF(uint32_t zz) {
    return ( (zz&0x000000ff)|MASK_BTIF);
}

uint8_t setBTE(uint32_t zz) {
    return ( (zz&0x000000ff)| MASK_BTE );
}

/**/

void setBtdPropInit( const char *cmd, const char *addr, const char *name, const char *devclass){
    property_set(PROP_CMD, cmd);
    property_set(PROP_ADR, addr);
    property_set(PROP_NAM, name);
    property_set(PROP_DCL, devclass);
}

void setBtdProp(char *cmd, char *addr, char *name, char *devclass){

    if (strcmp(PROP_CMD, cmd) ){
        property_set(PROP_CMD, cmd);
    }

    if (strcmp(PROP_NAM, name) ){
        property_set(PROP_NAM, name);
    }

    if (strcmp(PROP_ADR, addr) ){
        property_set(PROP_ADR, addr);
    }

    if (strcmp(PROP_DCL, devclass) ){
        property_set(PROP_DCL, devclass);
    }

    property_get(PROP_CMD, cmd, "-1");
    property_get(PROP_NAM, name, "aic");
    property_get(PROP_ADR, addr, "FF");
    property_get(PROP_DCL, devclass, "640");

    //BTDLOG("%s  -  %s - %s - %s", PROP_CMD ,PROP_ADR, PROP_NAM ,PROP_DCL);

}

void splitAction( char *action, char *cmd, char *addr, char *name, char *devclass ){

    //char str[80] = "-1:aic@FFFFF#20C";
    const char okCmd[2] = ":";
    const char okNam[2] = "@";
    const char okAdr[2] = "#";

    char *token;
    token = strtok(action, okCmd);strcpy(cmd ,token);
    token = strtok(NULL, okNam  );strcpy(name,token);
    token = strtok(NULL, okAdr  );strcpy(addr,token);
    token = strtok(NULL, okAdr  );strcpy(devclass,token);

    //BTDLOG("%s  -  %s - %s - %s", cmd, addr, name, devclass);
}

void setMSG ( uint8_t * addr,
              uint8_t * bd_name,
              uint8_t * devClass,
              uint8_t *typ,
              uint8_t *lenMsg      ,  void *buf )
{
    memcpy(buf, typ, sizeof(char));

    uint8_t *bd_addr=(uint8_t*)malloc(strlen(addr) + 1);
    sprintf(bd_addr,"@%s",addr);
    uint8_t *bd_devClass=(uint8_t*)malloc(strlen(devClass) + 1);
    sprintf(bd_devClass,"#%s",devClass);

    //name
    *lenMsg = *lenMsg + 1 ;
    uint8_t hdrlen=2;
    uint8_t lenName   = strlen((char*)bd_name);
    uint8_t lenAddr   = strlen((char*)bd_addr);
    uint8_t lenDevClass = strlen((char*)bd_devClass);

    memcpy((uint8_t*)buf+hdrlen, bd_name,  lenName ) ;

    //addr
    memcpy((uint8_t*)buf+hdrlen+lenName, bd_addr, lenAddr);

    //dev class
    memcpy((uint8_t*)buf+hdrlen+lenName+lenAddr, bd_devClass, lenDevClass);

    //header len
    *lenMsg = lenName + lenAddr + lenDevClass;
    memcpy((uint8_t*)buf+1, lenMsg, sizeof(char));
    *lenMsg += hdrlen;
}


int codeBT( btPayload * btData, void *buf)
{
    google::protobuf::uint32 cmd = 0 ;
    uint8_t typ;

    if(btData->has_btif()){
        cmd  = btData->btif().cmd();
    }else if ( btData->has_bte() ){
        cmd  = btData->bte().cmd();
    }

        //header typ
    if (cmd<100)
        typ = setBTIF(cmd)   ;
    else if (cmd<104)
        typ = setBTE(cmd-100);

    uint8_t * bd_name=(uint8_t*)malloc(512*sizeof(uint8_t) );// btData->name();
    uint8_t * bd_addr=(uint8_t*)malloc(512*sizeof(uint8_t) );// btData->addr();
    uint8_t * bd_devClass=(uint8_t*)malloc(512*sizeof(uint8_t) );// btData->devclass();

    if(btData->has_name())
        strcpy(reinterpret_cast<char*>(bd_name), btData->name().c_str() );
    else
        strncpy(reinterpret_cast<char*>(bd_name), "AiC", 3);

    if(btData->has_addr())
        strcpy(reinterpret_cast<char*>(bd_addr), btData->addr().c_str() );
    else
        strcpy(reinterpret_cast<char*>(bd_addr), "@0E0E0E0E0E0E");

    if(btData->has_devclass())
        strcpy(reinterpret_cast<char*>(bd_devClass), btData->devclass().c_str() );
    else
        strcpy(reinterpret_cast<char*>(bd_devClass), "#620");

    uint8_t len =0;
    setMSG (bd_addr,
            bd_name,
            bd_devClass,
            &typ   ,
            &len   ,  buf);

    ALOGE(" codeBT -buf=%s -len is %d", (char*)buf, len);
    ALOGE(" codeBT -btData->name()=%s -btData->addr()=%s -btData->devclass()=%s",btData->name().c_str(),btData->addr().c_str(),btData->devclass().c_str());

    return len;
}

google::protobuf::uint32 readHdr(char *buf)
{
    google::protobuf::uint32 size;
    google::protobuf::io::ArrayInputStream ais(buf,4);
    CodedInputStream coded_input(&ais);
    coded_input.ReadVarint32(&size);//Decode the HDR and get the size
    ALOGE(" readHdr --   size of payload is %d", size);
    return size;
}

uint8_t readBody(int csock,google::protobuf::uint32 siz , uint8_t* msg)
{

    unsigned int bytecount;
    btPayload  payload;
    char* buffer = (char*) calloc(siz+4, sizeof(char));//size of the payload and hdr
    //Read the entire buffer including the hdr
    if((bytecount = recv(csock, (void *)buffer, 4+siz, MSG_WAITALL))== -1)
    {
        ALOGE(" readBody: Error receiving data (%d)", errno);
        free((void*)buffer);
        return 0;
    }
    else if (bytecount != siz+4)
    {
        ALOGE(" readBody: Received the wrong payload size (expected %d, received %d)", siz+4, bytecount);
        free((void*)buffer);
        return 0;
    }
    ALOGE(" readBody --  Second read byte count is %d", bytecount);
    //Assign ArrayInputStream with enough memory
    google::protobuf::io::ArrayInputStream ais(buffer, siz+4);

    ALOGE(" readBody --  Assign ArrayInputStream with enough memory");


    CodedInputStream coded_input(&ais);
    //Read an unsigned integer with Varint encoding, truncating to 32 bits.
    ALOGE(" readBody --  Read an unsigned integer with Varint encoding, truncating to 32 bits.");
    coded_input.ReadVarint32(&siz);
    //After the message's length is read, PushLimit() is used to prevent the CodedInputStream
    //from reading beyond that length.Limits are used when parsing length-delimited
    //embedded messages
    ALOGE("After the message's length is read, PushLimit()");
    google::protobuf::io::CodedInputStream::Limit msgLimit = coded_input.PushLimit(siz);
    //De-Serialize
        ALOGE(" readBody --  De-Serialize");
    payload.ParseFromCodedStream(&coded_input);
    //Once the embedded message has been parsed, PopLimit() is called to undo the limit
    ALOGE(" readBody -- Once the embedded message has been parsed, PopLimit() ");
    coded_input.PopLimit(msgLimit);
    //Print the message

    // Code the message
    uint8_t len = codeBT( &payload, msg);
    ALOGE(" readBody --  Print the message %d - %d ", payload.has_name(), len);
    ALOGE(" readBody --  Print the message %s ", payload.name().c_str() );

  return len ;
}

int tcp_write_buff( void *data, int len)
{
    ALOGE(" tcp_write_buff in  ");
    struct sockaddr_in local_nfc_server;
    local_nfc_server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    local_nfc_server.sin_family = AF_INET;
    local_nfc_server.sin_port = htons(33800);
    int fd_local =0;
    fd_local = socket(AF_INET, SOCK_STREAM, 0);
//     for (int i=0;i<30;i++) {
    for (;;) {
        sleep(1);
        if(!connect(fd_local, (struct sockaddr *)&local_nfc_server, sizeof(local_nfc_server) ) )
            break;
    }

    int err = send(fd_local, data, len, 0);
    ALOGE(" tcp_write_buff out  ");
    close(fd_local);

    return (err);
}


static int start_server(uint16_t port) {
    int server = -1;
    struct sockaddr_in srv_addr;
    long haddr;

    bzero(&srv_addr, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    srv_addr.sin_port = htons(port);

    if ((server = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        SLOGE(" BTD Unable to create socket\n");
        return -1;
    }

    int yes =1 ;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(server, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
        SLOGE(" BTD Unable to bind socket, errno=%d\n", errno);
        return -1;
    }

    return server;
}

static int wait_for_client(int server) {
    int client = -1;

    if (listen(server, 1) < 0) {
        BTDLOG("Unable to listen to socket, errno=%d\n", errno);
        return -1;
    }

     struct timeval tv = { 1, 0 };  // 50 ms
     fd_set rfds;
     FD_ZERO(&rfds);
     FD_SET(server, &rfds);

     //BTDLOG("select\n");
     int iResult = select(server+1, &rfds, (fd_set *) 0, (fd_set *) 0, &tv);

    if(iResult > 0)
    {
        client = accept(server, NULL, 0);
        if (client < 0) {
            BTDLOG("Unable to accept socket for main conection, errno=%d\n", errno);
            return -1;
        }
    }else if(iResult==0){
        client = 0;
    }
    //BTDLOG("iResult %d client %d\n", iResult, client);
    return client;
}

void *hdl_player_conn(void * args){
    BTDLOG("");

    int *server = (int*)args;
    char inbuf[5];
    int client=0;
    int retry = 0;
    BTDLOG("%d" , (int)(*server));
    uint8_t  msg_len ;
    uint8_t * msg = (uint8_t*)calloc(1024, sizeof(uint8_t));
    unsigned char msg2[512] = {1 };

     while((client = wait_for_client(*server))!=-1)
     {
        sleep (1) ;
        //BTDLOG(" client accept %d", client);
        int rrr= recv( client, inbuf, 5, MSG_PEEK);
        if ( rrr > 0){
            msg_len = readBody(client,readHdr(inbuf), msg);
            int siz = tcp_write_buff(msg, (int)(msg_len) );
            SLOGD("msg_len=%d, siz= %d ; go fool bufff !!!", (int)msg_len, siz);
            //BTDLOG("debug recv");
            BTDLOG(":: debug recv %s", (char*)msg);
        }
    }//end while
    return NULL;
}

void *hdl_new_getprop()
{
    char action[512];

    int cmd = -1;
    int tmp=-1, i=-1;

    char bd_name[512] ;
    char bd_addr[512] ;
    char bd_class[512] ;
    uint8_t len = 0;
    uint8_t typ;

    void *buf = (void *) malloc (512 * sizeof(uint8_t));
    unsigned char msg2[512] = {1};

    char *s_cmd,  *addr,  *name,  *devclass;

    s_cmd=(char*)malloc(512*sizeof(uint8_t));
    name=(char*)malloc(512*sizeof(uint8_t));
    addr=(char*)malloc(512*sizeof(uint8_t));
    devclass=(char*)malloc(512*sizeof(uint8_t));

    setBtdPropInit("1", "0F0F", "AIC", "640");

    while(1){
        property_get(PROP_ACT, action, "-1:aic@FFFFF#20C");
        splitAction( action, s_cmd, addr, name, devclass);
        setBtdProp( s_cmd, addr, name, devclass);

        cmd=atoi(s_cmd);
        sleep(1);
        //BTDLOG(":: Waiting cmd=%d tmp=%d", cmd, tmp);

        if ( cmd != tmp ){  // Detect prop changing
            tmp=cmd;

            switch (cmd)
            {
                case -1:  //acl
                        BTDLOG(":: acl ");

                        strcpy(reinterpret_cast<char*>(msg2),"error");
                        len = strlen(reinterpret_cast<char*>(msg2));

                        memcpy(buf, &len, sizeof(char));
                        memcpy(buf+1, (void*)msg2, len+1);

                        i  = tcp_write_buff( buf, len+1);
                     break;

                default:
                        memset (bd_name, 0, 512);
                        memset (bd_addr, 0, 512);
                        memset (buf, 0, 512);
                        BTDLOG(":: default A ::%d %s", cmd , s_cmd);
                        strcpy(bd_name, name);

                        if (strlen(bd_name) > 8){
                            strncpy(bd_name,bd_name,8);
                            bd_name[8]='\0';
                        }

                        //header typ
                        if (cmd<100)
                            typ = setBTIF(cmd)   ;
                        else if (cmd<104)
                            typ = setBTE(cmd-100);

                        setMSG (    reinterpret_cast<uint8_t*>(addr),
                                    reinterpret_cast<uint8_t*>(bd_name),
                                    reinterpret_cast<uint8_t*>(devclass),
                                    &typ,
                                    &len,  buf);

                        //send
                        i  = tcp_write_buff(  buf, len);
                        BTDLOG(":: default C  %d %s %d",i, (char*)buf ,typ );
                    break;
            }//end switch
        }// end if cmd
    }
    return NULL;
}//end hdl_new_getprop

int main ( )
{
    int sim_server = -1;

    BTDLOG("\n:::::::::::::::::::::::::::::::::::::::::::::::::::");
    BTDLOG(":: Btd app starting");

    if ((sim_server = start_server(SIM_BT_PORT)) == -1)
    {
        BTDLOG(" BT Unable to create socket\n");
        return -1;
    }
    pthread_t *thread0;
    pthread_t *thread1;
    thread0 = (pthread_t *)malloc(sizeof(pthread_t));
    thread1 = (pthread_t *)malloc(sizeof(pthread_t));

    BTDLOG(":: Btd app terminating");

    int s  = pthread_create(thread0, NULL, &hdl_player_conn, (void*)&sim_server);
    int s1 = pthread_create(thread1, NULL, &hdl_new_getprop, NULL);

    pthread_join(*thread0, NULL);

    BTDLOG(":: Btd app terminating");

    return 0;
}
