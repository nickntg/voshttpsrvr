/*

     HTTP Server

     Function
     ---------------------------------------------------------------------
     This program acts as an http server.

     This program was based on the "multiple_tcp_accepts.c" program.
     ---------------------------------------------------------------------

     Revisions
     ---------------------------------------------------------------------
     08/10/2000   : Started working on it.
     09/10/2000   : Got it working, without replies.
     15/10/2000   : First basic version (GET only)
     ---------------------------------------------------------------------

     NTG

*/

#include <stdio.h>
#include <tcp_socket.h>
#include <ioctl.h>
#include <tcp_errno.h>
#include <time.h>

#define DEF_PROTOCOL    0
#define FLAG_BITS       0

#define CV              char_varying

#define MAX_SDS 16
#define BUFSIZE 4096

int max_socket = 0;
int num_sockets = 0;
int sd[MAX_SDS];

#define LISTEN   0

fd_set   active_sds;
fd_set   read_sds;

extern int errno;
char error_text[80];

int                 rBytes;
char                response[1024];
CV(256)             fName;
char                fileBuffer[BUFSIZE + 4];

short int ecode;

time_t timer;
struct tm *time_ptr;
char *time_string;

int http_readconf();
int http_request(long int *, char *, char *, long int *, char *);
void http_error_response(short int *, char *, long int *, char *);

void send_file(int);
int example_exit();
int find_unused_socket();
int close_socket();
void s$parse_command();
void  exit();

main()
{
    short                error_code;
    char_varying(256)    v_org_device, v_device;
    char                 device[256+1];
    char_varying(256)    v_iaddress;
    char                 iaddress[256+1];
    int                  port_no;
    struct sockaddr_in   bind_saddr, nsaddr;
    int                  nsaddr_sz;
    int                  rcvd_bytes;
    int                  rval, nfound, i;
    int                  listen_backlog;
    char                 buffer[BUFSIZE];
    int                  off = 0;

    error_code = http_readconf();
    if (error_code != 0)
          return;

    strcpy(v_iaddress, "0.0.0.0");

    if (get_device(device) < 0)
        example_exit("Error on get_device()");

    strcpy(v_org_device, device);
    strcpy(v_device, device);

    s$parse_command(&(char_varying(32))"HTTPServer", &error_code,
        &(char_varying)"option(-device), string, value", &v_device,
        &(char_varying)"option(-address), string, value", &v_iaddress,
        &(char_varying)"option(-port), number,longword, value, =0", &port_no,
        &(char_varying(32))"end");

    if (error_code != 0)
        example_exit("Error on s$parse_command()");

    if (strcmp(v_device, v_org_device))
    {
        strcpy(device, v_device);
        if (set_device(device) < 0)
            example_exit("Error on set_device()");
    }

    strcpy(iaddress, v_iaddress);

    for (i = 0; i < MAX_SDS; i++)
        sd[i] = 0;

    sd[LISTEN] = socket(AF_INET, SOCK_STREAM, DEF_PROTOCOL);
    if (sd[LISTEN] < 0)
        example_exit("Error on socket()");

    bind_saddr.sin_family = AF_INET;
    bind_saddr.sin_port = port_no;
    bind_saddr.sin_addr.s_addr = inet_addr(iaddress);
    if (bind_saddr.sin_addr.s_addr == -1)
        example_exit("Improper bind address");

    rval = bind(sd[LISTEN], (struct sockaddr *)&bind_saddr, sizeof(bind_saddr));
    if (rval < 0)
        example_exit("Error on bind()");

    listen_backlog = 5; 
    rval = listen(sd[LISTEN], listen_backlog);
    if (rval < 0)
        example_exit("Error on listen()");

    net_ioctl(sd[LISTEN], FIONBIO, (char *)&off);

    FD_SET(sd[LISTEN], &active_sds);
    num_sockets = 0;

    for (;;)
    {
        for (i = 0; i < MAX_SDS; i++)
            if (sd[i] != 0 && sd[i] > max_socket)
                max_socket = sd[i];
        
        read_sds = active_sds;

        nfound = select(max_socket + 1, &read_sds, (fd_set *)0,
                        (fd_set *)0, (struct timeval *)0);

        /* Values for nfound (if using select_with_events)

           -2 : VOS event has been notified
           -1 : Error
            0 : t/o - not applicable in our case since we don't use timeout
           >0 : Number of descriptors that are ready */

       if (time(&timer) == -1) {
          printf("Error on calling time function \n", ecode);
          example_exit("Program aborted");
       }
       if ( (time_ptr = localtime(&timer)) == NULL ) {
          printf("Error on calling localtime function \n", ecode);
          example_exit("Program aborted");
       }
       time_string = asctime(time_ptr);
       printf("\n%s",time_string);

       if (nfound == -2) {
           nfound = 0;
        }

        if (nfound < 0)
            example_exit("Error on select()");

        for (i = 0; nfound && i < MAX_SDS; i++)
        {
            if (FD_ISSET(sd[i], &read_sds))
            {
                if (i == LISTEN)
                {
                    int nsd, nsi;

                    nsaddr_sz = sizeof(struct sockaddr);
                    nsd = accept(sd[LISTEN], (struct sockaddr *) &nsaddr, &nsaddr_sz);
                    if (nsd < 0)
                    {
                        perror("Error on accept()");
                        continue;
                    }

                    nsi = find_unused_socket();
                    if (nsi < 0)
                    {
                        printf("Connection refused, too many sockets\n");
                        net_close(nsd); 
                        continue;
                    }

                    sethostent(-1);
                    if (gethostbyaddr((char *)(&nsaddr.sin_addr.s_addr),
                                      sizeof(u_long),AF_INET) == NULL) {
                        printf("Connection refused, unknown client\n");
                        printf("Client address was %s\n",
                                   inet_ntoa(nsaddr.sin_addr));
                        net_close(nsd);
                        endhostent();
                        continue; 
                    }            
                    endhostent();

                    sd[nsi] = nsd;

                    FD_SET(sd[nsi], &active_sds);
                    num_sockets++;

                    printf("New socket descriptor is %d.\n", sd[nsi]);
                    printf("Source address of new socket is %s\n",
                                   inet_ntoa(nsaddr.sin_addr));
                    printf("%d connected sockets.\n", num_sockets);
                    continue;
                }
                else
                {
                    rcvd_bytes = recv(sd[i], buffer, BUFSIZE, FLAG_BITS);
                    if (rcvd_bytes < 0)
                    {
                        sprintf(error_text,
                              "Error on recv() - socket %d", sd[i]);
                        perror(error_text);
                        close_socket(i);
                    }
                    else if (rcvd_bytes == 0)
                    {
                        shutdown(sd[i], READ_WRITE_SHUTDOWN);
                        num_sockets--;
                        printf("Closing socket %d.\n", sd[i]);
                        printf("%d connected sockets.\n", num_sockets);
                        close_socket(i);
                    }
                    else
                    {
                        rBytes = 0;
                        error_code = http_request(&rcvd_bytes, buffer, (char *)&fName, &rBytes, response);
                        if (error_code != 0) {
                              http_error_response(&error_code, (char *)&fName, &rBytes, response);
                              response[rBytes] = 0;
                              rval = tcp_send_data(i, response, rBytes);
                              if (rval >= 0) 
                                 close_socket(i);
                           }
                        else {
                           response[rBytes] = 0;
                           rval = tcp_send_data(i, response, rBytes);
                           if (rval >= 0)
                              send_file(i);
                        }
                    }
                }
                nfound--;
            }
        }
    }
}

void send_file(sckno)
int sckno;
{
int rval;
short int duration_switches;
short int io_type;
short int port_id;
short int buffer_length;
short int record_length;
short int ecode;
short int file_org;
short int max_length;
short int lock_mode;
short int access_mode;
CV(32) index_name;

     duration_switches = 0;
     file_org = 33;
     max_length = 0;
     io_type = 1;
     lock_mode = 2;
     access_mode = 1;
     strcpy(index_name, "");
     s$attach_port(&(CV(66))"", &fName, &duration_switches, &port_id, &ecode);
     if (ecode != 0)
          return;

     s$open(&port_id, &file_org, &max_length, &io_type, &lock_mode, &access_mode, &(CV(32))"", &ecode);
     if (ecode != 0) {
          s$detach_port(&port_id, &ecode);
          return;
     }

     while (ecode == 0) {
          buffer_length = BUFSIZE;
          s$read_raw(&port_id, &buffer_length, &record_length, fileBuffer, &ecode);
          if (ecode ==1025)
               record_length = 0;
          if ((ecode == 0) || (ecode == 1025)) {
               if (record_length != BUFSIZE) {
                    fileBuffer [++record_length] = 13;
                    fileBuffer [++record_length] = 10;
                    fileBuffer [++record_length] = 13;
                    fileBuffer [++record_length] = 10;
                    ecode = 1025;
               }
               rval = tcp_send_data(sckno, fileBuffer, record_length);
               if (rval < 0)
                    ecode = 1;
          }
     }
     s$close(&port_id, &ecode);
     s$detach_port(&port_id, &ecode);

     if (rval >= 0)
          close_socket(sckno);
}

int tcp_send_data (sckno, buffer, rlen)
int sckno, rlen;
char *buffer;
{
     int rvalcode;

     buffer[rlen] = 0;
     rvalcode = send(sd[sckno], buffer, rlen, MSG_BLOCKING);
     if (rvalcode < 0) {
          sprintf(error_text,"Error on send() - socket %d", sd[sckno]);
          perror(error_text);
          close_socket(sckno);
     }
     return(rvalcode);

}

int find_unused_socket()
{
     int  i;

     for (i = 0; i < MAX_SDS; i++)
          if (sd[i] == 0)
               return(i);
     return(-1);
}

close_socket(si)
int si;
{
    net_close(sd[si]);
    FD_CLR(sd[si], &active_sds);
    sd[si] = 0;
}

example_exit(exit_str)
char *exit_str;
{
    perror(exit_str);
    exit();
}
