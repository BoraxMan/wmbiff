/* tlsComm.c - primitive routines to aid TLS communication
   within wmbiff, without rewriting each mailbox access
   scheme.  These functions hide whether the underlying
   transport is encrypted.

   Neil Spring (nspring@cs.washington.edu) */

/* TODO: handle "* BYE" internally? */

#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#ifdef WITH_TLS
#include <gnutls.h>
#endif
#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#include "tlsComm.h"

/* this is the per-connection state that is maintained for
   each connection; BIG variables are for ssl (null if not
   used). */
#define BUF_SIZE 1024
struct connection_state {
	int sd;
	char *name;
#ifdef WITH_TLS
	GNUTLS_STATE state;
	X509PKI_CLIENT_CREDENTIALS xcred;
#else
	/*@null@ */ void *state;
	/*@null@ */ void *xcred;
#endif
	char unprocessed[BUF_SIZE];
};

/* gotta do our own line buffering, sigh */
static int
getline_from_buffer(char *readbuffer, char *linebuffer, int linebuflen);
void handle_gnutls_read_error(int readbytes, struct connection_state *scs);

void tlscomm_close(struct connection_state *scs)
{
#ifdef DEBUG_COMM
	fprintf(stderr, "%s: closing.\n",
			(scs->name != NULL) ? scs->name : "null");
#endif

	/* not ok to call this more than once */
	if (scs->state) {
#ifdef WITH_TLS
		gnutls_bye(scs->sd, scs->state, GNUTLS_SHUT_RDWR);
		gnutls_free_x509_client_sc(scs->xcred);
		gnutls_deinit(scs->state);
		scs->xcred = NULL;
#endif
	} else {
		(void) close(scs->sd);
	}
	scs->sd = -1;
	scs->state = NULL;
	scs->xcred = NULL;
	free(scs->name);
	scs->name = NULL;
	free(scs);
}

/* this avoids blocking without using non-blocking i/o */
static int wait_for_it(int sd, int timeoutseconds)
{
	fd_set readfds;
	struct timeval tv;
	tv.tv_sec = timeoutseconds;
	tv.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(sd, &readfds);
	if (select(sd + 1, &readfds, NULL, NULL, &tv) == 0) {
#ifdef DEBUG_COMM
		fprintf(stderr,
				"select timed out after %d seconds on socket: %d\n",
				timeoutseconds, sd);
#endif
		return (0);
	}
	return (FD_ISSET(sd, &readfds));
}

static int
getline_from_buffer(char *readbuffer, char *linebuffer, int linebuflen)
{
	/* TODO: respect linebuflen */
	char *p, *q;
	int i;
	/* find end of line */
	for (p = readbuffer, i = 0; *p != '\n' && *p != '\0'; p++, i++);
	if (i != 0) {
		/* grab the end of line too! */
		i++;
		/* copy a line into the linebuffer */
		strncpy(linebuffer, readbuffer, (size_t) i);
		/* sigh, null terminate */
		linebuffer[i] = '\0';
		/* shift the rest over; this could be done
		   instead with strcpy... I think. */
		q = readbuffer;
		if (*p != '\0') {
			p++;
			do {
				*(q++) = *(p++);
			} while (*p != '\0');
		}
		/* null terminate */
		*(q++) = *(p++);
		/* return the length of the line */
	}
	return i;
}

/* eat lines, until one starting with prefix is found;
   this skips 'informational' IMAP responses */
/* the correct response to a return value of 0 is almost
   certainly tlscomm_close(scs): don't _expect() anything
   unless anything else would represent failure */
int tlscomm_expect(struct connection_state *scs,
				   const char *prefix, char *buf, int buflen)
{
	int prefixlen = (int) strlen(prefix);
	memset(buf, 0, buflen);
#ifdef DEBUG_COMM
	fprintf(stderr, "%s: expecting: %s\n", scs->name, prefix);
#endif
	while (wait_for_it(scs->sd, 10)) {
		int readbytes;
#ifdef WITH_TLS
		if (scs->state) {
			readbytes =
				gnutls_read(scs->sd, scs->state, scs->unprocessed,
							BUF_SIZE);
			if (readbytes < 0) {
				handle_gnutls_read_error(readbytes, scs);
				return 0;
			}
		} else
#endif
		{
			readbytes = read(scs->sd, scs->unprocessed, BUF_SIZE);
			if (readbytes < 0) {
				fprintf(stderr, "%s: error reading: %s\n", scs->name,
						strerror(errno));
				return 0;
			}
		}
		if (readbytes == 0) {
			return 0;			/* bummer */
		} else
			while (readbytes >= prefixlen) {
				int linebytes;
				linebytes =
					getline_from_buffer(scs->unprocessed, buf, buflen);
				if (linebytes == 0) {
					readbytes = 0;
				} else {
					readbytes -= linebytes;
					if (strncmp(buf, prefix, prefixlen) == 0) {
#ifdef DEBUG_COMM
						fprintf(stderr, "%s: got: %*s", scs->name,
								readbytes, buf);
#endif
						return 1;	/* got it! */
					}
#ifdef DEBUG_COMM
					fprintf(stderr, "%s: dumped(%d/%d): %.*s", scs->name,
							linebytes, readbytes, linebytes, buf);
#endif
				}
			}
	}
	fprintf(stderr, "%s: expecting: '%s', saw '%s'\n", scs->name, prefix,
			buf);
	return 0;					/* wait_for_it failed */
}

int tlscomm_gets(char *buf, int buflen, struct connection_state *scs)
{
	return (tlscomm_expect(scs, "", buf, buflen));
}

void tlscomm_printf(struct connection_state *scs, const char *format, ...)
{
	va_list args;
	char buf[1024];
	int bytes;

	if (scs == NULL) {
		fprintf(stderr, "null connection to tlscomm_printf\n");
		abort();
	}
	va_start(args, format);
	bytes = vsnprintf(buf, 1024, format, args);
	va_end(args);

	if (scs->sd != -1) {
#ifdef WITH_TLS
		if (scs->state) {
			int written = gnutls_write(scs->sd, scs->state, buf, bytes);
			if (written < bytes) {
				fprintf(stderr, "Error %s prevented writing: %*s\n",
						gnutls_strerror(written), bytes, buf);
				return;
			}
		} else
#endif
			(void) write(scs->sd, buf, bytes);
	} else {
		fprintf(stderr,
				"warning: tlscomm_printf called with an invalid socket descriptor\n");
		return;
	}
#ifdef DEBUG_COMM
	fprintf(stderr, "wrote %*s", bytes, buf);
#endif
}

/* most of this file only makes sense if using TLS. */
#ifdef WITH_TLS

#ifdef DEBUG_COMM
/* taken from the GNUTLS documentation; edited to work
   on more recent versions of gnutls */
#define PRINTX(x,y) if (y[0]!=0) printf(" -   %s %s\n", x, y)
#define PRINT_DN(X) PRINTX( "CN:", X->common_name); \
	PRINTX( "OU:", X->organizational_unit_name); \
	PRINTX( "O:", X->organization); \
	PRINTX( "L:", X->locality_name); \
	PRINTX( "S:", X->state_or_province_name); \
	PRINTX( "C:", X->country);

static int print_info(GNUTLS_STATE state)
{
	const char *tmp;
	X509PKI_CLIENT_AUTH_INFO x509_info;
	const gnutls_DN *dn;

	/* print the key exchange's algorithm name
	 */
	tmp = gnutls_kx_get_name(gnutls_get_current_kx(state));
	printf("- Key Exchange: %s\n", tmp);

	/* in case of X509 PKI
	 */
	if (gnutls_get_auth_info_type(state) == GNUTLS_X509PKI) {
		x509_info = gnutls_get_auth_info(state);
		if (x509_info != NULL) {
			switch (gnutls_x509pki_client_get_peer_certificate_status
					(x509_info)) {
			case GNUTLS_CERT_NOT_TRUSTED:
				printf("- Peer's X509 Certificate was NOT verified\n");
				break;
			case GNUTLS_CERT_EXPIRED:
				printf
					("- Peer's X509 Certificate was verified but is expired\n");
				break;
			case GNUTLS_CERT_TRUSTED:
				printf("- Peer's X509 Certificate was verified\n");
				break;
			case GNUTLS_CERT_INVALID:
			default:
				printf("- Peer's X509 Certificate was invalid\n");
				break;

			}
			printf(" - Certificate info:\n");
			printf(" - Certificate version: #%d\n",
				   gnutls_x509pki_client_get_peer_certificate_version
				   (x509_info));

			dn = gnutls_x509pki_client_get_peer_dn(x509_info);
			PRINT_DN(dn);

			printf(" - Certificate Issuer's info:\n");
			dn = gnutls_x509pki_client_get_issuer_dn(x509_info);
			PRINT_DN(dn);
		}
	}

	tmp = gnutls_version_get_name(gnutls_get_current_version(state));
	printf("- Version: %s\n", tmp);

	tmp =
		gnutls_compression_get_name(gnutls_get_current_compression_method
									(state));
	printf("- Compression: %s\n", tmp);

	tmp = gnutls_cipher_get_name(gnutls_get_current_cipher(state));
	printf("- Cipher: %s\n", tmp);

	tmp = gnutls_mac_get_name(gnutls_get_current_mac_algorithm(state));
	printf("- MAC: %s\n", tmp);

	return 0;
}
#endif



struct connection_state *initialize_gnutls(int sd, char *name)
{
	static int gnutls_initialized;
	int zok;
	struct connection_state *ret = malloc(sizeof(struct connection_state));

	if (gnutls_initialized == 0) {
		gnutls_global_init();
		gnutls_initialized = 1;
	}

	assert(gnutls_init(&ret->state, GNUTLS_CLIENT) == 0);
	assert(gnutls_set_protocol_priority(ret->state, GNUTLS_TLS1,
										GNUTLS_SSL3, 0) == 0);
	assert(gnutls_set_cipher_priority(ret->state, GNUTLS_3DES_CBC,
									  GNUTLS_ARCFOUR, 0) == 0);
	assert(gnutls_set_compression_priority(ret->state, GNUTLS_ZLIB,
										   GNUTLS_NULL_COMPRESSION,
										   0) == 0);
	assert(gnutls_set_kx_priority(ret->state, GNUTLS_KX_RSA, 0) == 0);
	assert(gnutls_set_mac_priority(ret->state, GNUTLS_MAC_SHA,
								   GNUTLS_MAC_MD5, 0) == 0);

	/* no client private key */
	if (gnutls_allocate_x509_client_sc(&ret->xcred, 0) < 0) {
		fprintf(stderr, "gnutls memory error\n");
		exit(1);
	}

	/* TODO: 
	   zok =
	   gnutls_set_x509_client_trust(ret->xcred, "cafile.pem",
	   "crlfile.pem");
	   if (zok != 0) {
	   // printf("error setting client trust\n");
	   }
	 */

	gnutls_set_cred(ret->state, GNUTLS_X509PKI, ret->xcred);

	zok = gnutls_handshake(sd, ret->state);
	if (zok < 0) {
		fprintf(stderr, "%s: Handshake failed\n", name);
		fprintf(stderr, "%s: This may be a problem in gnutls, "
				"which is under development\n", name);
		fprintf(stderr,
				"%s: Specifically, problems have been found where the extnValue \n"
				"  buffer in _gnutls_get_ext_type() in lib/x509_extensions.c is too small in\n"
				"  gnutls versions up to 0.2.3.  This copy of wmbiff was compiled with \n"
				"  gnutls version %s.\n", name, LIBGNUTLS_VERSION);
		gnutls_perror(zok);
		gnutls_deinit(ret->state);
		free(ret);
		return (NULL);
	} else {
#ifdef DEBUG_COMM
		printf("%s: Handshake was completed\n", name);
		print_info(ret->state);
#endif
		ret->sd = sd;
		ret->name = name;
	}
	return (ret);
}

/* moved down here, to keep from interrupting the flow with
   verbose error crap */
void handle_gnutls_read_error(int readbytes, struct connection_state *scs)
{
	if (gnutls_is_fatal_error(readbytes) == 1) {
		fprintf(stderr,
				"%s: Received corrupted data(%d) - server has terminated the connection abnormally\n",
				scs->name, readbytes);
	} else {
		if (readbytes == GNUTLS_E_WARNING_ALERT_RECEIVED
			|| readbytes == GNUTLS_E_FATAL_ALERT_RECEIVED)
			printf("* Received alert [%d]\n",
				   gnutls_get_last_alert(scs->state));
		if (readbytes == GNUTLS_E_REHANDSHAKE)
			printf("* Received HelloRequest message\n");
	}
	fprintf(stderr, "%s: error reading: %s\n",
			scs->name, gnutls_strerror(readbytes));
}

#else
/* declare stubs when tls isn't compiled in */
struct connection_state *initialize_gnutls( /*@unused@ */ int sd,
										   /*@unused@ */ char *name)
{
	fprintf(stderr,
			"FATAL: tried to initialize ssl when ssl wasn't compiled in.\n");
	exit(EXIT_FAILURE);
}
#endif

/* either way: */
struct connection_state *initialize_unencrypted(int sd,
												/*@only@ */ char *name)
{
	struct connection_state *ret = malloc(sizeof(struct connection_state));
	assert(ret != NULL);
	ret->sd = sd;
	ret->name = name;
	ret->state = NULL;
	ret->xcred = NULL;
	return (ret);
}
