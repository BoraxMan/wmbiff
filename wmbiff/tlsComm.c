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

/* emulates what 'variadic macros' are great for */
#ifdef DEBUG_COMM
#define DM printf
#else
#define DM nullie
/*@unused@*/
static void nullie( /*@unused@ */ const char *format, ...)
{
	return;
}
#endif

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
	DM("%s: closing.\n", (scs->name != NULL) ? scs->name : "null");

	/* not ok to call this more than once */
	if (scs->state) {
#ifdef WITH_TLS
#if GNUTLS_VER >= 3
		gnutls_bye(scs->state, GNUTLS_SHUT_RDWR);
		gnutls_x509pki_free_sc(scs->xcred);
#else
		gnutls_bye(scs->sd, scs->state, GNUTLS_SHUT_RDWR);
		gnutls_free_x509_client_sc(scs->xcred);
#endif
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
		DM("select timed out after %d seconds on socket: %d\n",
		   timeoutseconds, sd);
		return (0);
	}
	return (FD_ISSET(sd, &readfds));
}

static int
getline_from_buffer(char *readbuffer, char *linebuffer, int linebuflen)
{
	char *p, *q;
	int i;
	/* find end of line (stopping if linebuflen is too small. */
	for (p = readbuffer, i = 0;
		 *p != '\n' && *p != '\0' && i < linebuflen - 1; p++, i++);

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
	DM("%s: expecting: %s\n", scs->name, prefix);
	while (wait_for_it(scs->sd, 10)) {
		int readbytes;
#ifdef WITH_TLS
		if (scs->state) {
			readbytes =
#if GNUTLS_VER >= 3
				gnutls_read(scs->state, scs->unprocessed, BUF_SIZE);
#else
				gnutls_read(scs->sd, scs->state, scs->unprocessed,
							BUF_SIZE);
#endif
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
						DM("%s: got: %*s", scs->name, readbytes, buf);
						return 1;	/* got it! */
					}
					DM("%s: dumped(%d/%d): %.*s", scs->name,
					   linebytes, readbytes, linebytes, buf);
				}
			}
	}
	fprintf(stderr, "%s: expecting: '%s', saw: %s", scs->name, prefix,
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
#if GNUTLS_VER>=3
			int written = gnutls_write(scs->state, buf, bytes);
#else
			int written = gnutls_write(scs->sd, scs->state, buf, bytes);
#endif
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
	DM("wrote %*s", bytes, buf);
}

/* most of this file only makes sense if using TLS. */
#ifdef WITH_TLS

#ifdef DEBUG_COMM
/* taken from the GNUTLS documentation, version 0.2.10; this
   may need to be updated from gnutls's cli.c if the gnutls interface
   changes, but that is only necessary if you want
   debug_comm. */
#define PRINTX(x,y) if (y[0]!=0) printf(" -   %s %s\n", x, y)
#define PRINT_DN(X) PRINTX( "CN:", X->common_name); \
	PRINTX( "OU:", X->organizational_unit_name); \
	PRINTX( "O:", X->organization); \
	PRINTX( "L:", X->locality_name); \
	PRINTX( "S:", X->state_or_province_name); \
	PRINTX( "C:", X->country); \
	PRINTX( "E:", X->email); \
	PRINTX( "SAN:", gnutls_x509pki_client_get_subject_dns_name(state))

static int print_info(GNUTLS_STATE state)
{
	const char *tmp;
	CredType cred;
	const gnutls_DN *dn;
	CertificateStatus status;


	tmp = gnutls_kx_get_name(gnutls_get_current_kx(state));
	printf("- Key Exchange: %s\n", tmp);

	cred = gnutls_get_auth_type(state);
	switch (cred) {
	case GNUTLS_ANON:
		printf("- Anonymous DH using prime of %d bits\n",
			   gnutls_anon_client_get_dh_bits(state));

	case GNUTLS_X509PKI:
		status = gnutls_x509pki_client_get_peer_certificate_status(state);
		switch (status) {
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
		case GNUTLS_CERT_NONE:
			printf("- Peer did not send any X509 Certificate.\n");
			break;
		case GNUTLS_CERT_INVALID:
			printf("- Peer's X509 Certificate was invalid\n");
			break;
		}

		if (status != GNUTLS_CERT_NONE && status != GNUTLS_CERT_INVALID) {
			printf(" - Certificate info:\n");
			printf(" - Certificate version: #%d\n",
				   gnutls_x509pki_client_get_peer_certificate_version
				   (state));

			dn = gnutls_x509pki_client_get_peer_dn(state);
			PRINT_DN(dn);

			dn = gnutls_x509pki_client_get_issuer_dn(state);
			printf(" - Certificate Issuer's info:\n");
			PRINT_DN(dn);
		}
	default:
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

	assert(sd >= 0);

	if (gnutls_initialized == 0) {
		assert(gnutls_global_init() == 0);
		gnutls_initialized = 1;
	}

	assert(gnutls_init(&ret->state, GNUTLS_CLIENT) == 0);
#if GNUTLS_VER>=3
	{
		const int protocols[] = { GNUTLS_TLS1, GNUTLS_SSL3, 0 };
		const int ciphers[] =
			{ GNUTLS_CIPHER_3DES_CBC, GNUTLS_CIPHER_ARCFOUR, 0 };
		const int compress[] = { GNUTLS_COMP_ZLIB, GNUTLS_COMP_NULL, 0 };
		const int key_exch[] = { GNUTLS_KX_X509PKI_RSA, 0 };
		const int mac[] = { GNUTLS_MAC_SHA, GNUTLS_MAC_MD5, 0 };
		assert(gnutls_protocol_set_priority(ret->state, protocols) == 0);
		assert(gnutls_cipher_set_priority(ret->state, ciphers) == 0);
		assert(gnutls_compression_set_priority(ret->state, compress) == 0);
		assert(gnutls_kx_set_priority(ret->state, key_exch) == 0);
		assert(gnutls_mac_set_priority(ret->state, mac) == 0);
		/* no client private key */
		if (gnutls_x509pki_allocate_sc(&ret->xcred, 0) < 0) {
			fprintf(stderr, "gnutls memory error\n");
			exit(1);
		}
		gnutls_cred_set(ret->state, GNUTLS_X509PKI, ret->xcred);
		gnutls_transport_set_ptr(ret->state, sd);
		do {
			zok = gnutls_handshake(ret->state);
		} while (zok == GNUTLS_E_INTERRUPTED || zok == GNUTLS_E_AGAIN);
	}
#else
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
	gnutls_set_cred(ret->state, GNUTLS_X509PKI, ret->xcred);
	zok = gnutls_handshake(sd, ret->state);
#endif

	/* TODO: 
	   zok =
	   gnutls_set_x509_client_trust(ret->xcred, "cafile.pem",
	   "crlfile.pem");
	   if (zok != 0) {
	   // printf("error setting client trust\n");
	   }
	 */

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
		DM("%s: Handshake was completed\n", name);
#ifdef DEBUG_COMM
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
			fprintf(stderr, "* Received alert [%d]\n",
#if GNUTLS_VER>=3
					gnutls_alert_get_last(scs->state));
#else
					gnutls_get_last_alert(scs->state));
#endif
		if (readbytes == GNUTLS_E_REHANDSHAKE)
			fprintf(stderr, "* Received HelloRequest message\n");
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
	assert(sd >= 0);
	assert(ret != NULL);
	ret->sd = sd;
	ret->name = name;
	ret->state = NULL;
	ret->xcred = NULL;
	return (ret);
}

/* bad seed connections that can't be setup */
/*@only@*/
struct connection_state *initialize_blacklist( /*@only@ */ char *name)
{
	struct connection_state *ret = malloc(sizeof(struct connection_state));
	assert(ret != NULL);
	ret->sd = -1;
	ret->name = name;
	ret->state = NULL;
	ret->xcred = NULL;
	return (ret);
}


int tlscomm_is_blacklisted(const struct connection_state *scs)
{
	return (scs != NULL && scs->sd == -1);
}
