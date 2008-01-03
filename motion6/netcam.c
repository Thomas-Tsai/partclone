/*
 *      netcam.c
 *
 *      Module for handling network cameras.
 *
 *      This code was inspired by the original netcam.c module
 *      written by Jeroen Vreeken and enhanced by several Motion
 *      project contributors, particularly Angel Carpintero and
 *      Christopher Price.
 *	
 *      Copyright 2005, William M. Brack
 *      This software is distributed under the GNU Public license
 *      Version 2.  See also the file 'COPYING'.
 *	
 *
 *      When a netcam has been configured, instead of using the routines
 *      within video.c (which handle a CCTV-type camera) the routines
 *      within this module are used.  There are only four entry points -
 *      one for "starting up" the camera (netcam_start), for "fetching a
 *      picture" from it (netcam_next), one for cleanup at the end of a
 *      run (netcam_cleanup), and a utility routine for receiving data
 *      from the camera (netcam_recv).
 *
 *      Two quite different types of netcams are handled.  The simplest
 *      one is the type which supplies a single JPEG frame each time it
 *      is accessed.  The other type is one which supplies an mjpeg
 *      stream of data.
 *
 *      For each of these cameras, the routinei taking care of the netcam
 *      will start up a completely separate thread (which I call the "camera
 *      handler thread" within subsequent comments).  For a streaming camera,
 *      this handler will receive the mjpeg stream of data from the camera,
 *      and save the latest complete image when it begins to work on the next
 *      one.  For the non-streaming version, this handler will be "triggered"
 *      (signalled) whenever the main motion-loop asks for a new image, and
 *      will start to fetch the next image at that time.  For either type,
 *      the most recent image received from the camera will be returned to
 *      motion.
 */
#include "motion.h"

#include <netdb.h>
#include <netinet/in.h>
#include <regex.h>                  /* For parsing of the URL */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "netcam_wget.h"

#define CONNECT_TIMEOUT        10   /* timeout on remote connection attempt */
#define READ_TIMEOUT            5   /* default timeout on recv requests */
#define MAX_HEADER_RETRIES      5   /* Max tries to find a header record */
#define CAMERA_WARNINGS         3   /* if debug_level >= this number print */
#define CAMERA_INFO             5   /* debug level to activate everything */
#define MINVAL(x, y) ((x) < (y) ? (x) : (y))

/*
 * The macro NETCAM_DEBUG is for development testing of this module.
 * The macro SETUP is to assure that "configuration-setup" type messages
 * are also printed when NETCAM_DEBUG is set.  Set the following #if to
 * 1 to enable it, or 0 (normal setting) to disable it.
 */
#define SETUP    ((cnt->conf.setup_mode) || (debug_level >= CAMERA_INFO))

/* These strings are used for the HTTP connection */
static const char    *connect_req = "GET %s HTTP/1.0\r\n"
                      "User-Agent: Motion-netcam/" VERSION "\r\n"
                      "Connection: close\r\n";

static const char    *connect_auth_req = "Authorization: Basic %s\r\n";

/*
 * The following three routines (netcam_url_match, netcam_url_parse and
 * netcam_url_free are for 'parsing' (i.e. separating into the relevant
 * components) the URL provided by the user.  They make use of regular
 * expressions (which is outside the scope of this module, so detailed
 * comments are not provided).  netcam_url_parse is called from netcam_start,
 * and puts the "broken-up" components of the URL into the "url" element of
 * the netcam_context structure.
 *
 * Note that the routines are not "very clever", but they work sufficiently
 * well for the limited requirements of this module.  The expression:
 *   (http)://(((.*):(.*))@)?([^/:]|[-.a-z0-9]+)(:([0-9]+))?($|(/[^:]*))
 * requires 
 *   1) a string which begins with 'http', followed by '://'
 *   2) optionally a '@' which is preceded by two strings
 *      (with 0 or more characters each) separated by a ':'
 *      [this is for an optional username:password]
 *   3) a string comprising alpha-numerics, '-' and '.' characters
 *      [this is for the hostname]
 *   4) optionally a ':' followed by one or more numeric characters
 *      [this is for an optional port number]
 *   5) finally, either an end of line or a series of segments,
 *      each of which begins with a '/', and contains anything
 *      except a ':'
 */

/**
 * netcam_url_match
 * 	
 *      Finds the matched part of a regular expression
 * 
 * Parameters:
 *
 *      m          A structure containing the regular expression to be used
 *      input      The input string
 *
 * Returns:        The string which was matched
 *
 */
static char *netcam_url_match(regmatch_t m, const char *input)
{
	char *match = NULL;
	int len;

	if (m.rm_so != -1) {
		len = m.rm_eo - m.rm_so;

		if ((match = (char *) malloc(len + 1)) != NULL) {
			strncpy(match, input + m.rm_so, len);
			match[len] = '\0';
		}
	}

	return (match);
}

/**
 * netcam_url_parse
 * 
 *      parses a string containing a URL into it's components
 * 
 * Parameters:
 *      parse_url          A structure which will receive the results
 *                         of the parsing
 *      text_url           The input string containing the URL
 *
 * Returns:                Nothing
 *
 */
static void netcam_url_parse(struct url_t *parse_url, char *text_url)
{
	char *s;
	int i;
	const char *re = "(http)://(((.*):(.*))@)?"
	                 "([^/:]|[-.a-z0-9]+)(:([0-9]+))?($|(/[^:]*))";
	regex_t pattbuf;
	regmatch_t matches[10];

	parse_url->service = NULL;
	parse_url->userpass = NULL;
	parse_url->host = NULL;
	parse_url->port = 80;
	parse_url->path = NULL;

	/*
	 * regcomp compiles regular expressions into a form that is
	 * suitable for regexec searches
	 * regexec matches the URL string against the regular expression
	 * and returns an array of pointers to strings matching each match
	 * within (). The results that we need are finally placed in parse_url
	 */
	if (!regcomp(&pattbuf, re, REG_EXTENDED | REG_ICASE)) {
		if (regexec(&pattbuf, text_url, 10, matches, 0) !=
		    REG_NOMATCH) {
			for (i = 0; i < 10; i++) {
				if ((s = netcam_url_match(matches[i],
				    text_url)) != NULL) {
					switch (i) {
						case 1:
							parse_url->service = s;
							break;
						case 3:
							parse_url->userpass = s;
							break;
						case 6:
							parse_url->host = s;
							break;
						case 8:
							parse_url->port =
							        atoi(s);
							free(s);
							break;
						case 9:
							parse_url->path = s;
							break;
						/* other components ignored */
						default:
							free(s);
							break;
					}
				}
			}
		}
	}

	regfree(&pattbuf);
}

/**
 * netcam_url_free
 *
 *      General cleanup of the URL structure, called from netcam_cleanup.
 *
 * Parameters:
 *
 *      parse_url       Structure containing the parsed data
 *
 * Returns:             Nothing
 *
 */
static void netcam_url_free(struct url_t *parse_url)
{
	if (parse_url->service) {
		free(parse_url->service);
		parse_url->service = NULL;
	}
	if (parse_url->userpass) {
		free(parse_url->userpass);
		parse_url->userpass = NULL;
	}
	if (parse_url->host) {
		free(parse_url->host);
		parse_url->host = NULL;
	}
	if (parse_url->path) {
		free(parse_url->path);
		parse_url->path = NULL;
	}
}

/**
 * netcam_check_content_length
 *
 * 	Analyse an HTTP-header line to see if it is a Content-length
 *
 * Parameters:
 *
 *      header          Pointer to a string containing the header line
 *
 * Returns:
 *      -1              Not a Content-length line
 *      >=0             Value of Content-length field
 *
 */
static long netcam_check_content_length(char *header)
{
	long            length;		/* note this is a long, not an int */
	
	if (!header_process(header, "Content-Length", header_extract_number,
	                     &length)) {
		return -1;
	}
	
	return length;
}

/**
 * netcam_check_content_type
 *
 * 	Analyse an HTTP-header line to see if it is a Content-type
 *
 * Parameters:
 *
 *      header          Pointer to a string containing the header line
 *
 * Returns:
 *      -1              Not a Content-type line
 *      0               Content-type not recognized
 *      1               image/jpeg
 *      2               multipart/x-mixed-replace or multipart/mixed
 *
 */
static int netcam_check_content_type(char *header)
{
	char *content_type = NULL;
	int ret;

	if (!header_process(header, "Content-type", http_process_type,
	                     &content_type))
		return -1;
	
	if (!strcmp(content_type, "image/jpeg")) {
		ret = 1;
	} else if (!strcmp(content_type, "multipart/x-mixed-replace") ||
	           !strcmp(content_type, "multipart/mixed")) {
		ret = 2;
	} else
		ret = 0;
	
	if (content_type)
		free(content_type);
	
	return ret;
}


/**
 * netcam_read_next_header
 *
 *      Read the next header record from the camera.
 *
 * Parameters
 *
 *      netcam          pointer to a netcam_context
 *
 * Returns:             0 for success, -1 if any error
 *
 */
static int netcam_read_next_header(netcam_context_ptr netcam)
{
	int retval;
	char *header;

	/*
	 * return if not connected
	 */
	if (netcam->sock == -1) return -1;
	/*
	 * We are expecting a header which *must* contain a mime-type of
	 * image/jpeg, and *might* contain a Content-Length.
	 *
	 * If this is a "streaming" camera, the header *must* be preceded
	 * by a "boundary" string.
	 * 
	 */
	netcam->caps.content_length = 0;
	/*
	 * If this is a "streaming" camera, the stream header must be
	 * preceded by a "boundary" string
	 */
	if (netcam->caps.streaming) {
		while (1) {
			retval = header_get(netcam, &header,
			                    HG_NONE);
			if (retval != HG_OK) {
				motion_log(netcam->cnt, LOG_ERR, 0,
				           "Error reading image header");
				free(header);
				return -1;
			}
			retval = (strstr(header, netcam->boundary) == NULL);
			free(header);
			if (!retval)
				break;
		}
	}
	while (1) {
		retval = header_get(netcam, &header,
		                     HG_NONE);
		if (retval != HG_OK) {
			motion_log(netcam->cnt, LOG_ERR, 0,
			           "Error reading image header");
			free(header);
			return -1;
		}
		if (*header == 0)
			break;
		if ((retval = netcam_check_content_type(header)) >= 0) {
			if (retval != 1) {
				motion_log(netcam->cnt, LOG_ERR, 0,
				           "Header not JPEG");
				free(header);
				return -1;
			}
		}
		if ((retval = (int) netcam_check_content_length(header)) > 0) {
			netcam->caps.content_length = 1;       /* set flag */
			netcam->receiving->content_length = (int) retval;
		}
		free(header);
	}
	
	if (debug_level > CAMERA_INFO)
		motion_log(netcam->cnt, -1, 0, "Found image header record");

	free(header);
	return 0;
}

/**
 * netcam_read_first_header
 *
 * This routine attempts to read a header record from the netcam.  If
 * successful, it analyses the header to determine whether the camera is
 * a "streaming" type.  If it is, the routine looks for the Boundary-string;
 * if found, it positions just past the string so that the image header can
 * be read.  It then reads the image header and continues processing that
 * header as well.
 *
 * If the camera does not appear to be a streaming type, it is assumed that the
 * header just read was the image header.  It is processed to determine whether
 * a Content-length is present.
 * 
 * After this processing, the routine returns to the caller.
 *
 * Parameters:
 *      netcam            Pointer to the netcam_context structure
 *
 * Returns:               Content-type code if successful, -1 if not
 * 
 */
static int netcam_read_first_header(netcam_context_ptr netcam)
{
	int retval = -2;                         /* "Unknown err" */
	int ret;
	int firstflag = 1;
	char *header;
	char *boundary;
	struct context *cnt = netcam->cnt;       /* for conf debug_level */

	/* Send the initial command to the camera */
	if (send(netcam->sock, netcam->connect_request,
	         strlen(netcam->connect_request), 0) < 0) {
		motion_log(cnt, LOG_ERR, 1, "Error sending 'connect' request");
		return -1;
	}
	/*
	 * We expect to get back an HTTP header from the camera.
	 * Successive calls to header_get will return each line
	 * of the header received.  We will continue reading until
	 * a blank line is received.
	 * 
	 * As we process the header, we are looking for either of
	 * header lines Content-type or Content-length.  Content-type
	 * is used to determine whether the camera is "streaming" or
	 * "non-streaming", and Content-length will be used to determine
	 * whether future reads of images will be controlled by the
	 * length specified before the image, or by a boundary string.
	 *
	 * The Content-length will only be present "just before" an
	 * image is sent (if it is present at all).  That means that, if
	 * this is a "streaming" camera, it will not be present in the
	 * "first header", but will occur later (after a boundary-string).
	 * For a non-streaming camera, however, there is no boundary-string,
	 * and the first header is, in fact, the only header.  In this case,
	 * there may be a Content-length.
	 * 
	 */
	while (1) {                                 /* 'Do forever' */
		ret = header_get(netcam, &header, HG_NONE);
		if (ret != HG_OK) {
			if (debug_level > 5)
				motion_log(cnt, LOG_ERR, 0,
				           "Error reading first header (%s)",
				           header);
			free(header);
			return -1;
		}
		if (firstflag) {
			if ((ret = http_result_code(header)) != 200) {
				if (debug_level > 5)
					motion_log(cnt, -1, 0,
				                "HTTP Result code %d",
				                ret);
				free(header);
				return ret;
			}
			firstflag = 0;
			free(header);
			continue;
		}
		if (*header == 0)                  /* blank line received */
			break;
			
		if (SETUP) {
			motion_log(cnt, LOG_DEBUG, 0, "Received first header");
		}
		
		/* Check if this line is the content type */
		if ((ret = netcam_check_content_type(header)) >= 0) {
			retval = ret;
			/*
			 * We are expecting to find one of three types:
			 * 'multipart/x-mixed-replace', 'multipart/mixed'
			 * or 'image/jpeg'.  The first two will be received
			 * from a streaming camera, and the third from a
			 * camera which provides a single frame only.
			 */
			switch (ret) {
				case 1:         /* not streaming */
					if (SETUP) {
						motion_log(cnt, LOG_DEBUG, 0,
						    "Non-streaming camera");
					}
					netcam->caps.streaming = 0;
					break;
				case 2:         /* streaming */
					if (SETUP) {
						motion_log(cnt, LOG_DEBUG, 0,
						    "Streaming camera");
					}
					netcam->caps.streaming = 1;
					if ((boundary = strstr(header,
					        "boundary="))) {
						/*
						 * on error recovery this
						 * may already be set
						 * */
						if (netcam->boundary)
							free(netcam->boundary);
							
						netcam->boundary =
						    strdup(boundary + 9);
						netcam->boundary_length =
						    strlen(netcam->boundary);
						
						if (SETUP) {
						   motion_log(cnt,
						      LOG_DEBUG, 0,
						      "Boundary string [%s]",
						      netcam->boundary);
						}
					}
					break;
				default:{               /* error */
						motion_log(cnt, LOG_ERR, 0,
						        "Unrecognized content "
						        "type");
						free(header);
						return -1;
					}
			}
		} else if ((ret = (int) netcam_check_content_length(header)) >= 0) {
			if (SETUP) {
				motion_log(cnt, LOG_DEBUG, 0,
				           "Content-length present");
			}
			netcam->caps.content_length = 1;	/* set flag */
			netcam->receiving->content_length = ret;
		}
		free(header);
	}
	free(header);

	return retval;
}

/**
 * netcam_disconnect
 *
 *      Disconnect from the network camera.
 *
 * Parameters:
 * 
 *      netcam  pointer to netcam context
 *
 * Returns:     Nothing
 *
 */
static void netcam_disconnect(netcam_context_ptr netcam)
{

	if (netcam->sock > 0) {
		if (close(netcam->sock) < 0)
			motion_log(netcam->cnt, LOG_ERR, 1,
			           "netcam_disconnect");
		netcam->sock = -1;
		rbuf_discard(netcam);
	}
}

/**
 * netcam_connect
 *
 *      Attempt to open the network camera as a stream device.
 *
 * Parameters:
 * 
 *      netcam  pointer to netcam_context structure
 *
 * Returns:     0 for success, -1 for error
 * 
 */
static int netcam_connect(netcam_context_ptr netcam, struct timeval *timeout)
{
	struct sockaddr_in server;      /* for connect */
	struct addrinfo *res;           /* for getaddrinfo */
	int ret;
	int saveflags;
	int back_err;
	socklen_t len;
	fd_set fd_w;
	struct timeval selecttime;

	/* Assure any previous connection has been closed */
	netcam_disconnect(netcam);

	/* create a new socket */
	if ((netcam->sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		motion_log(netcam->cnt, LOG_ERR, 1,
		           "Attempting to create socket");
		return -1;
	}
	/* lookup the hostname given in the netcam URL */
	if ((ret = getaddrinfo(netcam->connect_host, NULL, NULL, &res))
	     != 0) {
		motion_log(netcam->cnt, LOG_ERR, 0,
		           "getaddrinfo() failed (%s): %s",
		           netcam->connect_host,
		           gai_strerror(ret));
		netcam_disconnect(netcam);
		return -1;
	}

	/*
	 * Fill the hostname details into the 'server' structure and
	 * attempt to connect to the remote server
	 */
	memset(&server, 0, sizeof(server));
	memcpy(&server, res->ai_addr, sizeof(server));
	freeaddrinfo(res);

	server.sin_family = AF_INET;
	server.sin_port = htons(netcam->connect_port);
	if (timeout)
		netcam->timeout = *timeout;
	
	if ((setsockopt(netcam->sock, SOL_SOCKET, SO_RCVTIMEO, 
	                &netcam->timeout, sizeof(netcam->timeout))) < 0) {
		motion_log(netcam->cnt, LOG_ERR, 1, "setsockopt() failed");
		netcam_disconnect(netcam);
		return -1;
	}
	
	/*
	 * Unfortunately, the SO_RCVTIMEO does not affect the 'connect'
	 * call, so we need to be quite a bit more clever.  We want to
	 * use netcam->timeout, so we set the socket non-blocking and
	 * then use a 'select' system call to control the timeout.
	 */

	if ((saveflags = fcntl(netcam->sock, F_GETFL, 0)) < 0) {
		motion_log(netcam->cnt, LOG_ERR, 1, "fcntl(1) on socket");
		close(netcam->sock);
		netcam->sock = -1;
		return -1;
	}
	/* Set the socket non-blocking */
	if (fcntl(netcam->sock, F_SETFL, saveflags | O_NONBLOCK) < 0) {
		motion_log(netcam->cnt, LOG_ERR, 1, "fcntl(2) on socket");
		close(netcam->sock);
		netcam->sock = -1;
		return -1;
	}
	/* Now the connect call will return immediately */
	ret = connect(netcam->sock, (struct sockaddr *) &server,
	              sizeof(server));
	back_err = errno;           /* save the errno from connect */

	/* Restore the normal socket flags */
	if (fcntl(netcam->sock, F_SETFL, saveflags) < 0) {
		motion_log(netcam->cnt, LOG_ERR, 1, "fcntl(3) on socket");
		close(netcam->sock);
		netcam->sock = -1;
		return -1;
	}
	
	/* If the connect failed with anything except EINPROGRESS, error */
	if ((ret < 0) && (back_err != EINPROGRESS)) {
		motion_log(netcam->cnt, LOG_ERR, 1, "connect() failed (%d)",
		           back_err);
		close(netcam->sock);
		netcam->sock = -1;
		return -1;
	}

	/* Now we do a 'select' with timeout to wait for the connect */
	FD_ZERO(&fd_w);
	FD_SET(netcam->sock, &fd_w);
	selecttime.tv_sec = CONNECT_TIMEOUT;
	selecttime.tv_usec = 0;
	ret = select(FD_SETSIZE, NULL, &fd_w, NULL, &selecttime);

	if (ret == 0) {            /* 0 means timeout */
		motion_log(netcam->cnt, LOG_ERR, 0, "timeout on connect()");
		close(netcam->sock);
		netcam->sock = -1;
		return -1;
	}

	/*
	 * A +ve value returned from the select (actually, it must be a
	 * '1' showing 1 fd's changed) shows the select has completed.
	 * Now we must check the return code from the select.
	 */
	len = sizeof(ret);
	if (getsockopt(netcam->sock, SOL_SOCKET, SO_ERROR, &ret, &len) < 0) {
		motion_log(netcam->cnt, LOG_ERR, 1, "getsockopt after connect");
		netcam_disconnect(netcam);
		return -1;
	}
	
	/* If the return code is anything except 0, error on connect */
	if (ret) {
		motion_log(netcam->cnt, LOG_ERR, 0, "connect returned error");
		netcam_disconnect(netcam);
		return -1;
	}
	/* The socket info is stored in the rbuf structure of our context */
	rbuf_initialize(netcam);
	
	return 0;            /* success */
}


/**
 * netcam_check_buffsize
 *
 * This routine checks whether there is enough room in a buffer to copy
 * some additional data.  If there is not enough room, it will re-allocate
 * the buffer and adjust it's size.
 *
 * Parameters:
 *      buff            Pointer to a netcam_image_buffer structure
 *      numbytes        The number of bytes to be copied
 * 	
 * Returns:             Nothing
 */
static void
netcam_check_buffsize(netcam_context_ptr netcam, netcam_buff_ptr buff,
                      size_t numbytes)
{
	if ((buff->size - buff->used) >= numbytes)
		return;

	if (debug_level > CAMERA_INFO)
		motion_log(netcam->cnt, -1, 0,
		           "expanding buffer from %d to %d bytes\n",
		           (int) buff->size, (int) buff->size +
		                 NETCAM_BUFFSIZE);

	buff->ptr = myrealloc(netcam->cnt, buff->ptr, buff->size +
	                      NETCAM_BUFFSIZE,
	                      "netcam_check_buf_size");
	buff->size += NETCAM_BUFFSIZE;
}

/**
 * netcam_read_jpeg
 *
 * This routine reads a jpeg image from the netcam.  When it is called,
 * the stream is already positioned just after the image header.
 *
 * This routine is called under the four variations of two different
 * conditions:
 * 	1) Streaming or non-streaming camera
 * 	2) Header does or does not include Content-Length
 * Additionally, if it is a streaming camera, there must always be a
 * boundary-string.
 *
 * The routine will (attempt to) read the JPEG image.  If a Content-Length
 * is present, it will be used (this will result in more efficient code, and
 * also code which should be better at detecting and recovering from possible
 * error conditions).
 *
 * If a boundary-string is present (and, if the camera is streaming, this
 * *must* be the case), the routine will assure that it is recognized and
 * acted upon.
 *
 * Our algorithm for this will be as follows:
 * 	1) If a Content-Length is present, set the variable "remaining"
 * 	   to be equal to that value, else set it to a "very large"
 * 	   number.
 * 	2) While there is more data available from the camera:
 * 	   a) If there is a "boundary string" specified (from the initial
 * 	      header):
 * 	      i)   If the amount of data in the input buffer is less than
 * 	           the length of the boundary string, get more data into
 * 	           the input buffer (error if failure).
 * 	      ii)  If the boundary string is found, check how many
 * 	           characters remain in the input buffer before the start
 * 	           of the boundary string.  If that is less than the
 * 	           variable "remaining", reset "remaining" to be equal
 * 	           to that number.
 * 	   b) Try to copy up to "remaining" characters from the input
 * 	      buffer into our destination buffer.
 * 	   c) If there are no more characters available from the camera,
 * 	      exit this loop, else subtract the number of characters
 * 	      actually copied from the variable "remaining".
 * 	3) If Content-Length was present, and "remaining" is not equal
 * 	   to zero, generate a warning message for logging.
 *
 *
 * Parameters:
 *      netcam          Pointer to netcam context
 *
 *
 * Returns:             0 for success, -1 for error
 *
 */
static int netcam_read_jpeg(netcam_context_ptr netcam)
{
	netcam_buff_ptr buffer;
	size_t remaining;       /* # characters to read */
	size_t maxflush;        /* # chars before boundary */
	size_t rem, rlen, ix;   /* working vars */
	int retval;
	char *ptr, *bptr, *rptr;
	netcam_buff *xchg;
	/*
	 * Initialisation - set our local pointers to the context
	 * information
	 */
	buffer = netcam->receiving;
	/* Assure the target buffer is empty */
	buffer->used = 0;
	/* Prepare for read loop */
	if (buffer->content_length != 0)
		remaining = buffer->content_length;
	else
		remaining = 999999;

	/* Now read in the data */
	while (remaining) {
		/* Assure data in input buffer */
		if (netcam->response->buffer_left <= 0) {
			retval = rbuf_read_bufferful(netcam);
			if (retval <= 0)
				break;
			netcam->response->buffer_left = retval;
			netcam->response->buffer_pos = netcam->response->buffer;
		}
		/* If a boundary string is present, take it into account */
		bptr = netcam->boundary;
		if (bptr) {
			rptr = netcam->response->buffer_pos;
			rlen = netcam->response->buffer_left;
			/* Loop through buffer looking for start of boundary */
			while (1) {
				/*
				 * Logic gets a little complicated here.  The
				 * problem is that we are reading in input
				 * data in packets, and there is a (small)
				 * chance that the boundary string could be
				 * split across successive packets.
				 * First a quick check if the string *might*
				 * be in the current buffer.
				 */
				if (rlen > remaining)
					rlen = remaining;
				if (remaining < netcam->boundary_length)
					break;
				if ((ptr = memchr(rptr, *bptr, rlen)) == NULL)
					/* boundary not here (normal path) */
					break;
				/*
				 * At least the first char was found in the
				 * buffer - check for the rest
				 */
				rem = rlen - (ptr - rptr);
				for (ix = 1; (ix < rem) &&
				     (ix < netcam->boundary_length); ix++) {
					if (ptr[ix] != bptr[ix])
						break;
				}

				if ((ix != netcam->boundary_length) &&
				    (ix != rem)) {
					/*
					 * Not pointing at a boundary string -
					 * step along input
					 */
					ix = ptr - rptr + 1;
					rptr += ix;
					rlen -= ix;
					if (rlen <= 0)
						/*
						 * boundary not in buffer -
						 * go copy out
						 */
						break;
					/*
					 * not yet decided - continue
					 * through input
					 */
					continue;
				}
				/*
				 * If we finish the 'for' with 
				 * ix == boundary_length, that means we found
				 * the string, and should copy any data which
				 * precedes it into the target buffer, then
				 * exit the main loop.
				 */
				if (ix == netcam->boundary_length) {
					if ((ptr - netcam->response->buffer) <
					    (int) remaining)
						remaining = ptr -
						    netcam->response->buffer;
					/* go copy everything up to boundary */
					break;
				}

				/*
				 * If not, and ix == rem, that means we reached
				 * the end of the input buffer in the middle of
				 * our check, which is the (somewhat messy)
				 * problem mentioned above.
				 *
				 * Assure there is data before potential
				 * boundary string
				 */
				if (ptr != netcam->response->buffer) {
					/* 
					 * We have a boundary string crossing
					 * packets :-(. We will copy all the
					 * data up to the beginning of the
					 * potential boundary, then re-position
					 * the (partial) string to the
					 * beginning and get some more input
					 * data.  First we flush the input
					 * buffer up to the beginning of the
					 * (potential) boundary string
					 */
					ix = ptr - netcam->response->buffer_pos;
					netcam_check_buffsize(netcam, buffer,
					        ix);
					retval = rbuf_flush(netcam,
					        buffer->ptr + buffer->used, ix);
					buffer->used += retval;
					remaining -= retval;

					/*
					 * Now move the boundary fragment to
					 * the head of the input buffer.
					 * This is really a "hack" - ideally,
					 * we should have a function within the
					 * module netcam_wget.c to do this job!
					 */

					if (debug_level > CAMERA_INFO)
					    motion_log(netcam->cnt, -1, 0,
					         "Potential split boundary - "
					         "%d chars flushed, %d "
					         "re-positioned", ix,
					         (int) netcam->response->buffer_left);

					memmove(netcam->response->buffer, ptr,
					         netcam->response->buffer_left);
				}       /* end of boundary split over buffer */
				retval = netcam_recv(netcam,
				              netcam->response->buffer +
				              netcam->response->buffer_left,
				              sizeof(netcam->response->buffer) -
				              netcam->response->buffer_left,
				              NULL);
				
				if (retval <= 0) { /* this is a fatal error */
					motion_log(netcam->cnt, LOG_ERR, 1,
					   "recv() fail after boundary string");
					return -1;
				}
				/* Reset the input buffer pointers */
				netcam->response->buffer_left =
				        retval + netcam->response->buffer_left;
				netcam->response->buffer_pos =
				        netcam->response->buffer;
				/*
				 * This will cause a 'continue' of the
				 * main loop
				 */
				bptr = NULL;
				/*
				 * Return to do the boundary compare from
				 * the start
				 */
				break;
			}             /* end of while(1) input buffer search */

			/* !bptr shows we're processing split boundary */
			if (!bptr)
				continue;
		}             /* end of if(bptr) */
		/*
		 * boundary string not present, so just write out as much
		 * data as possible
		 */
		if (remaining) {
			maxflush = MINVAL(netcam->response->buffer_left,
			                  remaining);
			netcam_check_buffsize(netcam ,buffer, maxflush);
			retval = rbuf_flush(netcam, buffer->ptr +
			                    buffer->used, maxflush);
			buffer->used += retval;
			remaining -= retval;
		}
	}
	/*
	 * read is complete - set the current 'receiving' buffer atomically
	 * as 'latest', and make the buffer previously in 'latest' become
	 * the new 'receiving'
	 */
	if (gettimeofday(&netcam->receiving->image_time, NULL) < 0) {
		motion_log(netcam->cnt, LOG_ERR, 1,
		           "gettimeofday in netcam_read_jpeg");
	}
	pthread_mutex_lock(&netcam->mutex);
	xchg = netcam->latest;
	netcam->latest = netcam->receiving;
	netcam->receiving = xchg;
	pthread_mutex_unlock(&netcam->mutex);
	netcam->imgcnt++;
	
	if (!netcam->caps.streaming)
		netcam_disconnect(netcam);
	
	return 0;
}

/**
 * netcam_handler_loop
 *      This is the "main loop" for the handler thread.  It is created
 *      in netcam_start when a streaming camera is detected.
 *
 * Parameters
 *
 *      arg     Pointer to the motion context for this camera
 *
 * Returns:     NULL pointer
 *
 */
static void *netcam_handler_loop(void *arg)
{
	int retval;
	netcam_context_ptr netcam = arg;
	struct context *cnt = netcam->cnt; /* needed for the SETUP macro :-( */

	if (SETUP) {
		motion_log(cnt, LOG_INFO, 0,
		           "Camera handler thread [%d]  started",
		           netcam->threadnr);
	}

	/*
	 * The logic of our loop is very simple.  If this is a non-
	 * streaming camera, we re-establish connection with the camera
	 * and read the header record.  If it's a streaming camera, we
	 * position to the next "boundary string" in the input stream.
	 * In either case, we then read the following JPEG image into the
	 * next available buffer, updating the "next" and "latest" indices
	 * in our netcam * structure.  The loop continues until netcam->finish
	 * or cnt->finish is set.
	 */

	while (!netcam->finish) {
		if (!netcam->caps.streaming) {

			if (netcam_connect(netcam, NULL) < 0) {
				motion_log(cnt, LOG_ERR, 0,
				           "re-opening camera (non-streaming)");
				/* need to have a dynamic delay here */
				continue;
			}
			/* Send our request and look at the response */
			if ((retval = netcam_read_first_header(netcam)) != 1) {
				if (retval > 0) {
					motion_log(cnt, LOG_ERR, 0,
					           "Unrecognized image header "
					           "(%d)", retval);
				} else if (retval != -1) {
					motion_log(cnt, LOG_ERR, 0,
					           "Error in header (%d)",
					           retval);
				}
				/* need to have a dynamic delay here */
				continue;
			}
		} else {
			if (netcam_read_next_header(netcam) < 0) {
				if (netcam_connect(netcam, NULL) < 0) {
					motion_log(cnt, LOG_ERR, 0,
					           "re-opening camera "
					           "(streaming)");
					SLEEP(5,0);
					continue;
				}
				if ((retval = netcam_read_first_header(netcam)
				     != 2)) {
					if (retval > 0) {
						motion_log(cnt, LOG_ERR, 0,
						           "Unrecognized image "
						           "header "
						           "(%d)", retval);
					} else if (retval != -1) {
						motion_log(cnt,
						           LOG_ERR, 0,
						           "Error in header "
						           "(%d)", retval);
					}
					/* FIXME need some limit */
					continue;
				}
			}
		}
		if (netcam_read_jpeg(netcam) < 0)
			continue;
		/*
		 * FIXME
		 * Need to check whether the image was received / decoded
		 * satisfactorily
		 */
		
		/*
		 * If non-streaming, want to synchronize our thread with the
		 * motion main-loop.  
		 */
		if (!netcam->caps.streaming) {
			pthread_mutex_lock(&netcam->mutex);
			/* before anything else, check for system shutdown */
			if (netcam->finish) {
				pthread_mutex_unlock(&netcam->mutex);
				break;
			}
			/*
			 * If our current loop has finished before the next
			 * request from the motion main-loop, we do a
			 * conditional wait (wait for signal).  On the other
			 * hand, if the motion main-loop has already signalled
			 * us, we just continue.  In either event, we clear
			 * the start_capture flag set by the main loop.
			 */
			if (!netcam->start_capture)
				pthread_cond_wait(&netcam->cap_cond,
				                  &netcam->mutex);
			netcam->start_capture = 0;
			
			pthread_mutex_unlock(&netcam->mutex);
		}
	/* the loop continues forever, or until motion shutdown */
	}

	/* our thread is finished - decrement motion's thread count */
	pthread_mutex_lock(&global_lock);
	threads_running--;
	pthread_mutex_unlock(&global_lock);
	/* log out a termination message */
	motion_log(netcam->cnt, LOG_INFO, 0, "netcam camera handler: finish "
	           "set, exiting");
	/* setting netcam->thread_id to zero shows netcam_cleanup we're done */
	netcam->thread_id = 0;
	/* signal netcam_cleanup that we're all done */
	pthread_mutex_lock(&netcam->mutex);
	pthread_cond_signal(&netcam->exiting);
	pthread_mutex_unlock(&netcam->mutex);
	/* Goodbye..... */
	pthread_exit(NULL);
}

/**
 * netcam_recv
 *
 *      This routine receives the next block from the netcam.  It takes care
 *      of the potential timeouts and interrupt which may occur because of
 *      the settings from setsockopt.
 *
 * Parameters:
 *
 *      netcam          Pointer to a netcam context
 *      buffptr         Pointer to the receive buffer
 *      buffsize        Length of the buffer
 *      timeout         Pointer to struct timeval (NULL for no change)
 *
 * Returns:
 *      If successful, the length of the message received, otherwise the
 *      error reply from the system call.
 *
 */
ssize_t netcam_recv(netcam_context_ptr netcam, void *buffptr, size_t buffsize,
                    struct timeval *timeout) {
	size_t retval;
	if (timeout) {
		if ((timeout->tv_sec != netcam->timeout.tv_sec) ||
		    (timeout->tv_usec != netcam->timeout.tv_usec)) {
			if ((setsockopt(netcam->sock, SOL_SOCKET,
			     SO_RCVTIMEO, timeout, sizeof(*timeout))) < 0) {
				motion_log(netcam->cnt, LOG_ERR, 1,
				           "setsockopt() in netcam_recv "
				           "failed");
			}
		}
		netcam->timeout = *timeout;
	}
	do {
		retval = recv(netcam->sock, buffptr, buffsize, 0);
	} while ((retval == EINTR) || (retval == EAGAIN));
	
	return retval;
}

/**
 * netcam_cleanup
 *
 *      This routine releases any allocated data within the netcam context,
 *      then frees the context itself.  Extreme care must be taken to assure
 *      that the multi-threading nature of the program is correctly
 *      handled.
 *
 * Parameters:
 *
 *      netcam          Pointer to a netcam context
 *
 * Returns:             Nothing.
 *
 */
void netcam_cleanup(netcam_context_ptr netcam)
{
	struct timespec waittime;
	if (!netcam)
		return;
	/*
	 * This 'lock' is just a bit of "defensive" programming.  It should
	 * only be necessary if the routine is being called from different
	 * threads, but in our Motion design, it should only be called from
	 * the motion main-loop.
	 */
	pthread_mutex_lock(&netcam->mutex);
	if (netcam->cnt->netcam == NULL) {
		return;
	}
	/*
	 * We set the netcam_context pointer in the motion main-loop context
	 * to be NULL, so that this routine won't be called a second time
	 */
	netcam->cnt->netcam = NULL;
	/*
	 * Next we set 'finish' in order to get the camera-handler thread
	 * to stop.
	 */
	netcam->finish = 1;
	/*
	 * If the camera is non-streaming, the handler thread could be waiting
	 * for a signal, so we send it one.  If it's actually waiting on the
	 * condition, it won't actually start yet because we still have
	 * netcam->mutex locked.
	 */
	if (!netcam->caps.streaming) {
		pthread_cond_signal(&netcam->cap_cond);
	}
	
	/*
	 * Once the camera-handler gets to the end of it's loop (probably as
	 * soon as we release netcam->mutex), because netcam->finish has been
	 * set it will exit it's loop, do anything it needs to do with the
	 * netcam context, and then send *us* as signal (netcam->exiting).
	 * Note that when we start our wait on netcam->exiting, our lock on
	 * netcam->mutex is automatically released, which will allow the
	 * handler to complete it's loop, notice that 'finish' is set and exit.
	 * This should always work, but again (defensive programming) we
	 * use pthread_cond_timedwait and, if our timeout (3 seconds) expires
	 * we just do the cleanup the handler would normally have done.  This
	 * assures that (even if there is a bug in our code) motion will still
	 * be able to exit.
	 */
	waittime.tv_sec = time(NULL) + 8;   /* Seems that 3 is too small */
	waittime.tv_nsec = 0;
	if (pthread_cond_timedwait(&netcam->exiting, &netcam->mutex,
	                           &waittime) != 0) {
		/*
		 * Although this shouldn't happen, if it *does* happen we will
		 * log it (just for the programmer's information)
		 */
		motion_log(netcam->cnt, -1, 0, "No response from camera "
		           "handler - it must have already died");
		pthread_mutex_lock(&global_lock);
		threads_running--;
		pthread_mutex_unlock(&global_lock);
	}
	
	/* we don't need any lock anymore, so release it */
	pthread_mutex_unlock(&netcam->mutex);

	/* and cleanup the rest of the netcam_context structure */
	if (netcam->connect_host != NULL) {
		free(netcam->connect_host);
	}
	if (netcam->connect_request != NULL) {
		free(netcam->connect_request);
	}
	if (netcam->boundary != NULL) {
		free(netcam->boundary);
	}
	if (netcam->latest != NULL) {
		if (netcam->latest->ptr != NULL) {
			free(netcam->latest->ptr);
		}
		free(netcam->latest);
	}
	if (netcam->receiving != NULL) {
		if (netcam->receiving->ptr != NULL) {
			free(netcam->receiving->ptr);
		}
		free(netcam->receiving);
	}
	if (netcam->jpegbuf != NULL) {
		if (netcam->jpegbuf->ptr != NULL) {
			free(netcam->jpegbuf->ptr);
		}
		free(netcam->jpegbuf);
	}
	if (netcam->response != NULL) {
		free(netcam->response);
	}
	pthread_mutex_destroy(&netcam->mutex);
	pthread_cond_destroy(&netcam->cap_cond);
	pthread_cond_destroy(&netcam->exiting);
	free(netcam);
}

/**
 * netcam_next
 *
 *      This routine is called when the main 'motion' thread wants a new
 *      frame of video.  It fetches the most recent frame available from
 *      the netcam, converts it to YUV420P, and returns it to motion.
 *
 * Parameters:
 *      cnt             Pointer to the context for this thread
 *      image           Pointer to a buffer for the returned image
 *
 * Returns:             Pointer to the image, or NULL in case of error
 */
int netcam_next(struct context *cnt, unsigned char *image)
{
	netcam_context_ptr netcam;

	/*
	 * Here we have some more "defensive programming".  This check should
	 * never be true, but if it is just return with a "fatal error".
	 */
	if ((!cnt) || (!cnt->netcam))
		return -2;
	netcam = cnt->netcam;
	/*
	 * If we are controlling a non-streaming camera, we synchronize the
	 * motion main-loop with the camera-handling thread through a signal,
	 * together with a flag to say "start your next capture".
	 */
	if (!netcam->caps.streaming) {
		pthread_mutex_lock(&netcam->mutex);
		netcam->start_capture = 1;
		pthread_cond_signal(&netcam->cap_cond);
		pthread_mutex_unlock(&netcam->mutex);
	}
	/*
	 * If an error occurs in the JPEG decompression which follows this,
	 * jpeglib will return to the code within this 'if'.  Basically, our
	 * approach is to just return a NULL (failed) to the caller (an
	 * error message has already been produced by the libjpeg routines)
	 */
	if (setjmp(netcam->setjmp_buffer)) {
		return NETCAM_GENERAL_ERR | NETCAM_JPEG_CONV_ERR;
	}
	/* If there was no error, process the latest image buffer */
	return netcam_proc_jpeg(netcam, image);
}

/**
 * netcam_start
 *
 *      This routine is called from the main motion thread.  It's job is
 *      to open up the requested camera device and do any required
 *      initialisation.  If the camera is a streaming type, then this
 *      routine must also start up the camera-handling thread to take
 *      care of it.
 *
 * Parameters:
 *
 *      cnt     Pointer to the motion context structure for this device
 *
 * Returns:     0 on success, -1 on any failure
 * 
 */

int netcam_start(struct context *cnt)
{
	netcam_context_ptr netcam;        /* local pointer to our context */
	pthread_attr_t handler_attribute; /* attributes of our handler thread */
	int ix, retval;                   /* working var */
	char *ptr;                        /* working var */
	char *userpass;                   /* temp pointer to config value */
	char *encuserpass;                /* temp storage for encoded ver */
	char *request_pass = NULL;        /* temp storage for base64 conv */
	struct url_t url;                 /* for parsing netcam_proxy */

	if (debug_level > CAMERA_INFO)
		motion_log(cnt, -1, 0, "entered netcam_start()");

	memset(&url, 0, sizeof(url));
	if (SETUP) {
		motion_log(cnt, LOG_INFO, 0, "Camera thread starting...");
	}

	/*
	 * Create a new netcam_context for this camera
	 * and clear all the entries.
	 */
	cnt->netcam = (struct netcam_context *) mymalloc(cnt,
	              sizeof(struct netcam_context));
	memset(cnt->netcam, 0, sizeof(struct netcam_context));
	netcam = cnt->netcam;           /* Just for clarity in remaining code */
	netcam->cnt = cnt;              /* Fill in the "parent" info */

	/*
	 * Fill in our new netcam context with all known initial
	 * values.
	 */
	/* First the http context structure */
	netcam->response = (struct rbuf *) mymalloc(cnt, sizeof(struct rbuf));
	memset(netcam->response, 0, sizeof(struct rbuf));

	/* Next our image buffers */
	netcam->latest = mymalloc(cnt, sizeof(netcam_buff));
	memset(netcam->latest, 0, sizeof(netcam_buff));
	netcam->latest->ptr = mymalloc(cnt, NETCAM_BUFFSIZE);

	netcam->receiving = mymalloc(cnt, sizeof(netcam_buff));
	memset(netcam->receiving, 0, sizeof(netcam_buff));
	netcam->receiving->ptr = mymalloc(cnt, NETCAM_BUFFSIZE);

	netcam->jpegbuf = mymalloc(cnt, sizeof(netcam_buff));
	memset(netcam->jpegbuf, 0, sizeof(netcam_buff));
	netcam->jpegbuf->ptr = mymalloc(cnt, NETCAM_BUFFSIZE);

	netcam->timeout.tv_sec = READ_TIMEOUT;
	/*
	 * If a proxy has been specified, parse that URL.
	 */
	if (cnt->conf.netcam_proxy) {
		netcam_url_parse(&url, cnt->conf.netcam_proxy);
		if (!url.host) {
			motion_log(cnt, LOG_ERR, 0, "Invalid netcam_proxy (%s)",
			           cnt->conf.netcam_proxy);
			netcam_url_free(&url);
			return -1;
		}
		if (url.userpass) {
			motion_log(cnt, LOG_ERR, 0,
			           "Username/password not allowed on a "
			           "proxy URL");
			netcam_url_free(&url);
			return -1;
		}
		/*
		 * A 'proxy' means that our eventual 'connect' to our
		 * camera must be sent to the proxy, and that our 'GET' must
		 * include the full path to the camera host.
		 */
		netcam->connect_host = url.host;
		url.host = NULL;
		netcam->connect_port = url.port;
		netcam_url_free(&url);  /* Finished with proxy */
	}

	/*
	 * Parse the URL from the configuration data
	 */
	netcam_url_parse(&url, cnt->conf.netcam_url);
	if (!url.host) {
		motion_log(cnt, LOG_ERR, 0, "Invalid netcam_url (%s)",
		           cnt->conf.netcam_url);
		netcam_url_free(&url);
		return -1;
	}

	if (cnt->conf.netcam_proxy == NULL) {
		netcam->connect_host = url.host;
		url.host = NULL;
		netcam->connect_port = url.port;
	}

	/*
	 * The network camera may require a username and password.  If
	 * so, the information can come from two different places in the
	 * motion configuration file.  Either it can be present in
	 * the netcam_userpass, or it can be present as a part of the URL
	 * for the camera.  We assume the first of these has a higher
	 * relevance.
	 */
	if (cnt->conf.netcam_userpass)
		ptr = cnt->conf.netcam_userpass;
	else
		ptr = url.userpass;

	/*
	 * base64_encode needs up to 3 additional chars
	 */
	if (ptr) {
		userpass = mymalloc(cnt, strlen(ptr) + 3);
		strcpy(userpass, ptr);
	} else
		userpass = NULL;
	/*
	 * Now we want to create the actual string which will be used to
	 * connect to the camera.  It may or may not contain a username /
	 * password.  We first compose a basic connect message, then check
	 * whether a username / password is required and, if so, just
	 * concatenate it with the request.
	 * 
	 */
	/* space for final \r\n plus string terminator */
	ix = 3;

	/* See if username / password is required */
	if (userpass) {         /* if either of the above are non-NULL */
		/* Allocate space for the base64-encoded string */
		encuserpass = mymalloc(cnt,
		              BASE64_LENGTH(strlen(userpass)) + 1);
		/* Fill in the value */
		base64_encode(userpass, encuserpass, strlen(userpass));
		/* Now create the last part (authorization) of the request */
		request_pass = mymalloc(cnt, strlen(connect_auth_req) +
		                        strlen(encuserpass) + 1);
		ix += sprintf(request_pass, connect_auth_req, encuserpass);
		/* free the working variables */
		free(encuserpass);
	}
	/*
	 * We are now ready to set up the netcam's "connect request".  Most of
	 * this comes from the (preset) string 'connect_req', but additional
	 * characters are required if there is a proxy server, or if there is
	 * a username / password for the camera.  The variable 'ix' currently
	 * has the number of characters required for username/password (which
	 * could be zero) and for the \r\n and string terminator.  We will also
	 * always need space for the netcam path, and if a proxy is being used
	 * we also need space for a preceding  'http://{hostname}' for the
	 * netcam path.
	 */
	if (cnt->conf.netcam_proxy) {
		/*
		 * Allocate space for a working string to contain the path.
		 * The extra 4 is for "://" and string terminator.
		 */
		ptr = mymalloc(cnt, strlen(url.service) + strlen(url.host)
		               + strlen(url.path) + 4);
		sprintf(ptr, "http://%s%s", url.host, url.path);
	} else {
		/* if no proxy, set as netcam_url path */
		ptr = url.path;
		/*
		 * after generating the connect message the string
		 * will be freed, so we don't want netcam_url_free
		 * to free it as well.
		 */
		url.path = NULL;
	}
	ix += strlen(ptr);
	/*
	 * Now that we know how much space we need, we can allocate space
	 * for the connect-request string.
	 */
	netcam->connect_request = mymalloc(cnt, strlen(connect_req) + ix);
	/* Now create the request string with an sprintf */
	sprintf(netcam->connect_request, connect_req, ptr);
	if (userpass) {
		strcat(netcam->connect_request, request_pass);
		free(request_pass);
		free(userpass);
	}
	/* put on the final CRLF onto the request */
	strcat(netcam->connect_request, "\r\n");
	free(ptr);
	netcam_url_free(&url);         /* Cleanup the url data */

	/*
	 * Our basic initialisation has been completed.  Now we will attempt
	 * to connect with the camera so that we can then get a "header"
	 * in order to find out what kind of camera we are dealing with,
	 * as well as what are the picture dimensions.  Note that for
	 * this initial connection, any failure will cause an error
	 * return from netcam_start (unlike later possible attempts at
	 * re-connecting, if the network connection is later interrupted).
	 */
	for (ix = 0; ix < MAX_HEADER_RETRIES; ix++) {
		/*
		 * netcam_connect does an automatic netcam_close, so it's
		 * safe to include it as part of this loop
		 */
		if (netcam_connect(netcam, NULL) != 0) {
			motion_log(cnt, LOG_ERR, 0,
			           "Failed to open camera - "
			           "check your config");
			/* Fatal error on startup */
			ix = MAX_HEADER_RETRIES;
			break;;
		}
		if (netcam_read_first_header(netcam) >= 0)
			break;
		motion_log(cnt, LOG_ERR, 0,
		           "Error reading first header - re-trying");
	}

	if (ix == MAX_HEADER_RETRIES) {
		motion_log(cnt, LOG_ERR, 0,
		           "Failed to read first camera header - giving up");
		return -1;
	}
	/*
	 * If this is a streaming camera, we need to position just
	 * past the boundary string and read the image header
	 */
	if (netcam->caps.streaming) {
		if (netcam_read_next_header(netcam) < 0) {
			motion_log(cnt, LOG_ERR, 0,
			           "Failed to read first stream header - "
			           "giving up");
			return -1;
		}
	}

	/*
	 * We expect that, at this point, we should be positioned to read
	 * the first image available from the camera (directly after the
	 * applicable header).  We want to decode the image in order to get
	 * the dimensions (width and height).  If successful, we will use
	 * these to set the required image buffer(s) in our netcam_struct.
	 */
	if ((retval = netcam_read_jpeg(netcam)) != 0) {
		motion_log(cnt, LOG_ERR, 0,
		           "Failed trying to read first image");
		return -1;
	}

	/*
	 * If an error occurs in the JPEG decompression which follows this,
	 * jpeglib will return to the code within this 'if'.  If such an error
	 * occurs during startup, we will just abandon this attempt.
	 */
	if (setjmp(netcam->setjmp_buffer)) {
		motion_log(cnt, LOG_ERR, 0, "libjpeg decompression failure "
		                            "on first frame - giving up!");
		return -1;
	}
	netcam_get_dimensions(netcam);

	/* Fill in camera details into context structure */
	cnt->imgs.width = netcam->width;
	cnt->imgs.height = netcam->height;
	cnt->imgs.size = (netcam->width * netcam->height * 3) / 2;
	cnt->imgs.motionsize = netcam->width * netcam->height;
	cnt->imgs.type = VIDEO_PALETTE_YUV420P;

	/*
	 * Everything is now ready - start up the
	 * "handler thread".
	 */
	pthread_mutex_init(&netcam->mutex, NULL);
	pthread_cond_init(&netcam->cap_cond, NULL);
	pthread_cond_init(&netcam->exiting, NULL);
	pthread_attr_init(&handler_attribute);
	pthread_attr_setdetachstate(&handler_attribute,
	                             PTHREAD_CREATE_DETACHED);
	pthread_mutex_lock(&global_lock);
	netcam->threadnr = ++threads_running;
	pthread_mutex_unlock(&global_lock);
	if ((retval = pthread_create(&netcam->thread_id,
	                             &handler_attribute,
	                             &netcam_handler_loop, netcam)) < 0) {
		motion_log(cnt, LOG_ERR, 1,
		           "Starting camera handler thread [%d]",
		           netcam->threadnr);
		return -1;
	}

	return 0;
}
