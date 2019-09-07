/**************************************************************
 * Instituto Tecnológico de Costa Rica
 * Área Académica de Ing. en Computadores
 * CE4303 - Principios de Sistemas Operativos
 * Tarea 1. Filtrador y clasificador de imágenes como servicio
 * 
 **************************************************************/


/* 
 * Daemon and server merged to process images. 
 * Got daemon from: https://github.com/jirihnidek/daemon.git
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <wand/MagickWand.h>
#include <netinet/in.h>
#include <sys/select.h>

#define EOL "\r\n"
#define EOL_SIZE 2

static int running = 0;
static int delay = 1;
static int counter = 0;
static char *conf_file_name = NULL;
static char *pid_file_name = NULL;
static int pid_fd = -1;
static char *app_name = NULL;
static FILE *log_stream;

typedef struct
{
  char *ext;
  char *mediatype;
} extn;

//Possible media types
extn extensions[] = {
    {"gif", "image/gif"},
    {"jpg", "image/jpg"},
    {"jpeg", "image/jpeg"},
    {"png", "image/png"},
    {0, 0}};

/*
 A helper function
 */
void error(const char *msg)
{
  perror(msg);
  exit(1);
}
/*
  Function that applies the histogram equalization to the received image
*/
MagickWandGenesis();
      MagickWand *wand = NULL;
      wand = NewMagickWand();
      MagickReadImage(wand,ifile);
      MagickEqualizeImage(wand);
      MagickQuantizeImage(
                        wand,            // MagickWand
                        number_colors,   // Target number colors
                        RGBColorspace,  // Colorspace
                        treedepth,       // Optimal depth
                        MagickTrue,      // Dither
                        MagickFalse      // Quantization error
                    );
      MagickWriteImage(wand,"out.jpeg");
      if(wand)wand = DestroyMagickWand(wand);
      MagickWandTerminus();
/*
Function to recieve an image
*/
int receive_image(int socket)
{
  int fd = 0, confd = 0, b, tot;
  struct sockaddr_in serv_addr;

  char buff[1025];
  int num;
  int buffersize = 0, recv_size = 0, size = 0, read_size, write_size, packet_index = 1, stat;

  char imagearray[10241], verify = '1';
  FILE *image;

  //Find the size of the image
  do
  {
    stat = read(socket, &size, sizeof(int));
  } while (stat < 0);

  printf("Packet received.\n");
  printf("Packet size: %i\n", stat);
  printf("Image size: %i\n", size);
  printf(" \n");

  char buffer[] = "Got it";

  //Send our verification signal
  do
  {
    stat = write(socket, &buffer, sizeof(int));
  } while (stat < 0);

  printf("Reply sent\n");
  printf(" \n");

  FILE *fp = fopen("provacopy.png", "wb");
  tot = 0;
  char *httpresponse;
  if (fp != NULL)
  {
    while ((b = recv(socket, buff, 1024, 0)) > 0)
    { 
      httpresponse = strstr(b,"\r\n\r\n");
      if(httpresponse)
      {
        httpresponse +=4;
        tot += b;
        fwrite(buff, 1, b, fp);
      }
    }

    printf("Received byte: %d\n", tot);
    if (b < 0)
      perror("Receiving");

    fclose(fp);
  }
  else
  {
    perror("File");
  }
  printf("Done receiving the file\n");
  close(socket);
}

/*
 A helper function
 */
int get_file_size(int fd)
{
  struct stat stat_struct;
  if (fstat(fd, &stat_struct) == -1)
    return (1);
  return (int)stat_struct.st_size;
}

/*
 A helper function
 */
void send_new(int fd, char *msg)
{
  int len = strlen(msg);
  if (send(fd, msg, len, 0) == -1)
  {
    printf("Error in send\n");
  }
}

/*
 This function recieves the buffer
 until an "End of line(EOL)" byte is recieved
 */
int recv_new(int fd, char *buffer)
{
  char *p = buffer;              // Use of a pointer to the buffer rather than dealing with the buffer directly
  int eol_matched = 0;           // Use to check whether the recieved byte is matched with the buffer byte or not
  while (recv(fd, p, 1, 0) != 0) // Start receiving 1 byte at a time
  {
    if (*p == EOL[eol_matched]) // if the byte matches with the first eol byte that is '\r'
    {
      ++eol_matched;
      if (eol_matched == EOL_SIZE) // if both the bytes matches with the EOL
      {
        *(p + 1 - EOL_SIZE) = '\0'; // End the string
        return (strlen(buffer));    // Return the bytes recieved
      }
    }
    else
    {
      eol_matched = 0;
    }
    p++; // Increment the pointer to receive next byte
  }
  return (0);
}

/*
 A helper function: Returns the
 web root location.
 */
char *webroot()
{
  // open the file "conf" for reading
  FILE *in = fopen("conf", "rt");
  // read the first line from the file
  char buff[1000];
  fgets(buff, 1000, in);
  // close the stream
  fclose(in);
  char *nl_ptr = strrchr(buff, '\n');
  if (nl_ptr != NULL)
    *nl_ptr = '\0';
  return strdup(buff);
}

/*
 Handles php requests
 */
void php_cgi(char *script_path, int fd)
{
  send_new(fd, "HTTP/1.1 200 OK\n Server: Web Server in C\n Connection: close\n");
  dup2(fd, STDOUT_FILENO);
  char script[500];
  strcpy(script, "SCRIPT_FILENAME=");
  strcat(script, script_path);
  putenv("GATEWAY_INTERFACE=CGI/1.1");
  putenv(script);
  putenv("QUERY_STRING=");
  putenv("REQUEST_METHOD=GET");
  putenv("REDIRECT_STATUS=true");
  putenv("SERVER_PROTOCOL=HTTP/1.1");
  putenv("REMOTE_HOST=127.0.0.1");
  execl("/usr/bin/php-cgi", "php-cgi", NULL);
}

/*
 This function parses the HTTP requests,
 arrange resource locations,
 check for supported media types,
 serves files in a web root,
 sends the HTTP error codes.
 */
int connection(int fd)
{
  char request[500], resource[500], *ptr;
  int fd1, length;
  if (recv_new(fd, request) == 0)
  {
    printf("Recieve Failed\n");
  }
  printf("%s\n", request);
  // Check for a valid browser request
  ptr = strstr(request, " HTTP/");
  if (ptr == NULL)
  {
    printf("NOT HTTP !\n");
  }
  else
  {
    *ptr = 0;
    ptr = NULL;

    if (strncmp(request, "GET ", 4) == 0)
    {
      ptr = request + 4;
    }
    if(strncmp(request, "POST ", 4) == 0)
    {
      receive_image(fd);
    }
    if (ptr == NULL)
    {
      printf("Unknown Request ! \n");
    }
    else
    {
      if (ptr[strlen(ptr) - 1] == '/')
      {
        strcat(ptr, "index.html");
      }
      strcpy(resource, webroot());
      strcat(resource, ptr);
      char *s = strchr(ptr, '.');
      int i;
      for (i = 0; extensions[i].ext != NULL; i++)
      {
        if (strcmp(s + 1, extensions[i].ext) == 0)
        {
          fd1 = open(resource, O_RDONLY, 0);
          printf("Opening \"%s\"\n", resource);
          if (fd1 == -1)
          {
            printf("404 File not found Error\n");
            send_new(fd, "HTTP/1.1 404 Not Found\r\n");
            send_new(fd, "Server : Web Server in C\r\n\r\n");
            send_new(fd, "<html><head><title>404 Not Found</head></title>");
            send_new(fd, "<body><p>404 Not Found: The requested resource could not be found!</p></body></html>");
            //Handling php requests
          }
          else if (strcmp(extensions[i].ext, "php") == 0)
          {
            php_cgi(resource, fd);
            sleep(1);
            close(fd);
            exit(1);
          }
          else
          {
            printf("200 OK, Content-Type: %s\n\n",
                   extensions[i].mediatype);
            send_new(fd, "HTTP/1.1 200 OK\r\n");
            send_new(fd, "Server : Web Server in C\r\n\r\n");
            if (ptr == request + 4) // if it is a GET request
            {
              if ((length = get_file_size(fd1)) == -1)
                printf("Error in getting size !\n");
              size_t total_bytes_sent = 0;
              ssize_t bytes_sent;
              while (total_bytes_sent < length)
              {
                //Zero copy optimization
                if ((bytes_sent = sendfile(fd, fd1, 0,
                                           length - total_bytes_sent)) <= 0)
                {
                  if (errno == EINTR || errno == EAGAIN)
                  {
                    continue;
                  }
                  perror("sendfile");
                  return -1;
                }
                total_bytes_sent += bytes_sent;
              }
            }
          }
          break;
        }
        int size = sizeof(extensions) / sizeof(extensions[0]);
        if (i == size - 2)
        {
          printf("415 Unsupported Media Type\n");
          send_new(fd, "HTTP/1.1 415 Unsupported Media Type\r\n");
          send_new(fd, "Server : Web Server in C\r\n\r\n");
          send_new(fd, "<html><head><title>415 Unsupported Media Type</head></title>");
          send_new(fd, "<body><p>415 Unsupported Media Type!</p></body></html>");
        }
      }
      printf(&fd1);
      close(fd);
    }
  }
  shutdown(fd, SHUT_RDWR);
}

/**
 * \brief Read configuration from config file
 */
int read_conf_file(int reload)
{
	FILE *conf_file = NULL;
	int ret = -1;

	if (conf_file_name == NULL) return 0;

	conf_file = fopen(conf_file_name, "r");

	if (conf_file == NULL) {
		syslog(LOG_ERR, "Can not open config file: %s, error: %s",
				conf_file_name, strerror(errno));
		return -1;
	}

	ret = fscanf(conf_file, "%d", &delay);

	if (ret > 0) {
		if (reload == 1) {
			syslog(LOG_INFO, "Reloaded configuration file %s of %s",
				conf_file_name,
				app_name);
		} else {
			syslog(LOG_INFO, "Configuration of %s read from file %s",
				app_name,
				conf_file_name);
		}
	}

	fclose(conf_file);

	return ret;
}

/**
 * \brief This function tries to test config file
 */
int test_conf_file(char *_conf_file_name)
{
	FILE *conf_file = NULL;
	int ret = -1;

	conf_file = fopen(_conf_file_name, "r");

	if (conf_file == NULL) {
		fprintf(stderr, "Can't read config file %s\n",
			_conf_file_name);
		return EXIT_FAILURE;
	}

	ret = fscanf(conf_file, "%d", &delay);

	if (ret <= 0) {
		fprintf(stderr, "Wrong config file %s\n",
			_conf_file_name);
	}

	fclose(conf_file);

	if (ret > 0)
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}

/**
 * \brief Callback function for handling signals.
 * \param	sig	identifier of signal
 */
void handle_signal(int sig)
{
	if (sig == SIGINT) {
		fprintf(log_stream, "Debug: stopping daemon ...\n");
		/* Unlock and close lockfile */
		if (pid_fd != -1) {
			lockf(pid_fd, F_ULOCK, 0);
			close(pid_fd);
		}
		/* Try to delete lockfile */
		if (pid_file_name != NULL) {
			unlink(pid_file_name);
		}
		running = 0;
		/* Reset signal handling to default behavior */
		signal(SIGINT, SIG_DFL);
	} else if (sig == SIGHUP) {
		fprintf(log_stream, "Debug: reloading daemon config file ...\n");
		read_conf_file(1);
	} else if (sig == SIGCHLD) {
		fprintf(log_stream, "Debug: received SIGCHLD signal\n");
	}
}

/**
 * \brief This function will daemonize this app
 */
static void daemonize()
{
	pid_t pid = 0;
	int fd;

	/* Fork off the parent process */
	pid = fork();

	/* An error occurred */
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}

	/* Success: Let the parent terminate */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* On success: The child process becomes session leader */
	if (setsid() < 0) {
		exit(EXIT_FAILURE);
	}

	/* Ignore signal sent from child to parent process */
	signal(SIGCHLD, SIG_IGN);

	/* Fork off for the second time*/
	pid = fork();

	/* An error occurred */
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}

	/* Success: Let the parent terminate */
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* Set new file permissions */
	umask(0);

	/* Change the working directory to the root directory */
	/* or another appropriated directory */
	chdir("/");

	/* Close all open file descriptors */
	for (fd = sysconf(_SC_OPEN_MAX); fd > 0; fd--) {
		close(fd);
	}

	/* Reopen stdin (fd = 0), stdout (fd = 1), stderr (fd = 2) */
	stdin = fopen("/dev/null", "r");
	stdout = fopen("/dev/null", "w+");
	stderr = fopen("/dev/null", "w+");

	/* Try to write PID of daemon to lockfile */
	if (pid_file_name != NULL)
	{
		char str[256];
		pid_fd = open(pid_file_name, O_RDWR|O_CREAT, 0640);
		if (pid_fd < 0) {
			/* Can't open lockfile */
			exit(EXIT_FAILURE);
		}
		if (lockf(pid_fd, F_TLOCK, 0) < 0) {
			/* Can't lock file */
			exit(EXIT_FAILURE);
		}
		/* Get current PID */
		sprintf(str, "%d\n", getpid());
		/* Write PID to lockfile */
		write(pid_fd, str, strlen(str));
	}
}

/**
 * \brief Print help for this application
 */
void print_help(void)
{
	printf("\n Usage: %s [OPTIONS]\n\n", app_name);
	printf("  Options:\n");
	printf("   -h --help                 Print this help\n");
	printf("   -c --conf_file filename   Read configuration from the file\n");
	printf("   -t --test_conf filename   Test configuration file\n");
	printf("   -l --log_file  filename   Write logs to the file\n");
	printf("   -d --daemon               Daemonize this application\n");
	printf("   -p --pid_file  filename   PID file used by daemonized app\n");
	printf("\n");
}

/* Main function */
int main(int argc, char *argv[])
{
	static struct option long_options[] = {
		{"conf_file", required_argument, 0, 'c'},
		{"test_conf", required_argument, 0, 't'},
		{"log_file", required_argument, 0, 'l'},
		{"help", no_argument, 0, 'h'},
		{"daemon", no_argument, 0, 'd'},
		{"pid_file", required_argument, 0, 'p'},
		{NULL, 0, 0, 0}
	};
	int value, option_index = 0, ret;
	char *log_file_name = NULL;
	int start_daemonized = 0;
	/* Try to process all command line arguments */
	while ((value = getopt_long(argc, argv, "c:l:t:p:dh", long_options, &option_index)) != -1) {
		switch (value) {
			case 'c':
				conf_file_name = strdup(optarg);
				break;
			case 'l':
				log_file_name = strdup(optarg);
				break;
			case 'p':
				pid_file_name = strdup(optarg);
				break;
			case 't':
				return test_conf_file(optarg);
			case 'd':
				start_daemonized = 1;
				break;
			case 'h':
				print_help();
				return EXIT_SUCCESS;
			case '?':
				print_help();
				return EXIT_FAILURE;
			default:
				break;
		}
	}

	/* When daemonizing is requested at command line. */
	if (start_daemonized == 1) {
		/* It is also possible to use glibc function deamon()
		 * at this point, but it is useful to customize your daemon. */
		daemonize();
	}

	/* Open system log and write message to it */
	openlog(argv[0], LOG_PID|LOG_CONS, LOG_DAEMON);
	syslog(LOG_INFO, "Started %s", app_name);

	/* Daemon will handle two signals */
	signal(SIGINT, handle_signal);
	signal(SIGHUP, handle_signal);

	/* Try to open log file to this daemon */
	if (log_file_name != NULL) {
		log_stream = fopen(log_file_name, "a+");
		if (log_stream == NULL) {
			syslog(LOG_ERR, "Can not open log file: %s, error: %s",
				log_file_name, strerror(errno));
			log_stream = stdout;
		}
	} else {
		log_stream = stdout;
	}

	/* Read configuration from config file */
	read_conf_file(0);




	int sockfd, newsockfd, portno, pid;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;

	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");
	bzero((char *)&serv_addr, sizeof(serv_addr));
	portno = 1717;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR on binding");
	listen(sockfd, 5);
	clilen = sizeof(cli_addr);
	/* This global variable can be changed in function handling signal */
	running = 1;
	/*
	Server runs forever, forking off a separate
	process for each connection.
	*/
	fprintf(log_stream,"Entering while\n");
	while (running == 1)
	{
		newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
		if (newsockfd < 0)
		error("ERROR on accept");
		pid = fork();
		if (pid < 0)
		error("ERROR on fork");
		if (pid == 0)
		{
		close(sockfd);
		connection(newsockfd);
		exit(0);
		}
		else
		close(newsockfd);
	} /* end of while */
	close(sockfd);

	/* Close log file, when it is used. */
	if (log_stream != stdout) {
		fclose(log_stream);
	}

	/* Write system log and close it. */
	syslog(LOG_INFO, "Stopped %s", app_name);
	closelog();

	/* Free allocated memory */
	if (conf_file_name != NULL) free(conf_file_name);
	if (log_file_name != NULL) free(log_file_name);
	if (pid_file_name != NULL) free(pid_file_name);

	return EXIT_SUCCESS;
}
