/* tlsComm.c - primitive routines to aid TLS communication
   within wmbiff, without rewriting each mailbox access
   scheme.  These functions hide whether the underlying
   transport is encrypted.

   Neil Spring (nspring@cs.washington.edu) */

/* TODO: handle "* BYE" internally? */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#ifdef HAVE_GNUTLS_GNUTLS_H
#define USE_GNUTLS
#include <gnutls/gnutls.h>
#endif
#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif

#include "tlsComm.h"

#include "Client.h"				/* debugging messages */

/* if non-null, set to a file for certificate verification */
extern const char *certificate_filename;
/* if set, don't fail when dealing with a bad certificate.
   (continue to whine, though, as bad certs should be fixed) */
extern int SkipCertificateCheck;

/* WARNING: implcitly uses scs to gain access to the mailbox
   that holds the per-mailbox debug flag. */
#define TDM(lvl, args...) DM(scs->pc, lvl, "comm: " args)

/* how long to wait for the server to do its thing
   when we issue it a command (in seconds) */
#define EXPECT_TIMEOUT 40

/* this is the per-connection state that is maintained for
   each connection; BIG variables are for ssl (null if not
   used). */
#define BUF_SIZE 1024
struct connection_state {
	int sd;
	char *name;
#ifdef USE_GNUTLS
	GNUTLS_STATE state;
	GNUTLS_CERTIFICATE_CLIENT_CREDENTIALS xcred;
#else
	/*@null@ */ void *state;
	/*@null@ */ void *xcred;
#endif
	char unprocessed[BUF_SIZE];
	Pop3 pc;					/* mailbox handle for debugging messages */
};

/* gotta do our own line buffering, sigh */
static int
getline_from_buffer(char *readbuffer, char *linebuffer, int linebuflen);
void handle_gnutls_read_error(int readbytes, struct connection_state *scs);

void tlscomm_close(struct connection_state *scs)
{
	TDM(DEBUG_INFO, "%s: closing.\n",
		(scs->name != NULL) ? scs->name : "null");

	/* not ok to call this more than once */
	if (scs->state) {
#ifdef USE_GNUTLS
		gnutls_bye(scs->state, GNUTLS_SHUT_RDWR);
		gnutls_certificate_free_sc(scs->xcred);
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
	int ready_descriptors;
	tv.tv_sec = timeoutseconds;
	tv.tv_usec = 0;
	FD_ZERO(&readfds);
	FD_SET(sd, &readfds);
	ready_descriptors = select(sd + 1, &readfds, NULL, NULL, &tv);
	if (ready_descriptors == 0) {
		DMA(DEBUG_INFO,
			"select timed out after %d seconds on socket: %d\n",
			timeoutseconds, sd);
		return (0);
	} else if (ready_descriptors == -1) {
		DMA(DEBUG_ERROR,
			"select failed on socket %d: %s\n", sd, strerror(errno));
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
		/* advance past the newline */
		p++;
		/* copy a line into the linebuffer */
		strncpy(linebuffer, readbuffer, (size_t) i);
		/* sigh, null terminate */
		linebuffer[i] = '\0';
		/* shift the rest over; this could be done
		   instead with strcpy... I think. */
		q = readbuffer;
		if (*p != '\0') {
			while (*p != '\0') {
				*(q++) = *(p++);
			}
		}
		/* null terminate */
		*(q++) = *(p++);
		/* return the length of the line */
	}
	if (i < 0 || i > linebuflen) {
		DM((Pop3) NULL, DEBUG_ERROR, "bork bork bork!: %d %d\n", i,
		   linebuflen);
	}
	return i;
}

/* eat lines, until one starting with prefix is found;
   this skips 'informational' IMAP responses */
/* the correct response to a return value of 0 is almost
   certainly tlscomm_close(scs): don't _expect() anything
   unless anything else would represent failure */
int tlscomm_expect(struct connection_state *scs,
				   const char *prefix, char *linebuf, int buflen)
{
	int prefixlen = (int) strlen(prefix);
	int readbytes = -1;
	memset(linebuf, 0, buflen);
	TDM(DEBUG_INFO, "%s: expecting: %s\n", scs->name, prefix);
	while (wait_for_it(scs->sd, EXPECT_TIMEOUT)) {
#ifdef USE_GNUTLS
		if (scs->state) {
			/* BUF_SIZE - 1 leaves room for trailing \0 */
			readbytes =
				gnutls_read(scs->state, scs->unprocessed, BUF_SIZE - 1);
			if (readbytes < 0) {
				handle_gnutls_read_error(readbytes, scs);
				return 0;
			}
			if (readbytes > BUF_SIZE) {
				TDM(DEBUG_ERROR, "%s: unexpected bork!: %d %s\n",
					scs->name, readbytes, strerror(errno));
			}
		} else
#endif
		{
			readbytes = read(scs->sd, scs->unprocessed, BUF_SIZE - 1);
			if (readbytes < 0) {
				TDM(DEBUG_ERROR, "%s: error reading: %s\n", scs->name,
					strerror(errno));
				return 0;
			}
			if (readbytes >= BUF_SIZE) {
				TDM(DEBUG_ERROR, "%s: unexpected read bork!: %d %s\n",
					scs->name, readbytes, strerror(errno));
			}
		}
		/* force null termination */
		scs->unprocessed[readbytes] = '\0';
		if (readbytes == 0) {
			return 0;			/* bummer */
		} else
			while (readbytes >= prefixlen) {
				int linebytes;
				linebytes =
					getline_from_buffer(scs->unprocessed, linebuf, buflen);
				if (linebytes == 0) {
					readbytes = 0;
				} else {
					readbytes -= linebytes;
					if (strncmp(linebuf, prefix, prefixlen) == 0) {
						TDM(DEBUG_INFO, "%s: got: %*s", scs->name,
							readbytes, linebuf);
						return 1;	/* got it! */
					}
					TDM(DEBUG_INFO, "%s: dumped(%d/%d): %.*s", scs->name,
						linebytes, readbytes, linebytes, linebuf);
				}
			}
	}
	if (readbytes == -1) {
		TDM(DEBUG_INFO, "%s: timed out while expecting '%s'\n",
			scs->name, prefix);
	} else {
		TDM(DEBUG_ERROR, "%s: expecting: '%s', saw (%d): %s\n",
			scs->name, prefix, readbytes, linebuf);
	}
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
		DMA(DEBUG_ERROR, "null connection to tlscomm_printf\n");
		abort();
	}
	va_start(args, format);
	bytes = vsnprintf(buf, 1024, format, args);
	va_end(args);

	if (scs->sd != -1) {
#ifdef USE_GNUTLS
		if (scs->state) {
			int written = gnutls_write(scs->state, buf, bytes);
			if (written < bytes) {
				TDM(DEBUG_ERROR,
					"Error %s prevented writing: %*s\n",
					gnutls_strerror(written), bytes, buf);
				return;
			}
		} else
#endif
			(void) write(scs->sd, buf, bytes);
	} else {
		printf
			("warning: tlscomm_printf called with an invalid socket descriptor\n");
		return;
	}
	TDM(DEBUG_INFO, "wrote %*s", bytes, buf);
}

/* most of this file only makes sense if using TLS. */
#ifdef USE_GNUTLS
#include "gnutls-common.h"

static void
bad_certificate(const struct connection_state *scs, const char *msg)
{
	TDM(DEBUG_ERROR, "%s", msg);
	if (!SkipCertificateCheck) {
		TDM(DEBUG_ERROR, "to ignore this error, run wmbiff "
			"with the -skip-certificate-check option");
		exit(1);
	}
}

/* a start of a hack at verifying certificates.  does not
   provide any security at all.  I'm waiting for either
   gnutls to make this as easy as it should be, or someone
   to port Andrew McDonald's gnutls-for-mutt patch.
*/
int tls_check_certificate(struct connection_state *scs,
						  const char *remote_hostname)
{
	GNUTLS_CertificateStatus certstat;
	const gnutls_datum *cert_list;
	int cert_list_size = 0;

	if (gnutls_auth_get_type(scs->state) != GNUTLS_CRD_CERTIFICATE) {
		bad_certificate(scs, "Unable to get certificate from peer.\n");
	}
	certstat = gnutls_certificate_verify_peers(scs->state);
	if (certstat ==
		(GNUTLS_CertificateStatus) GNUTLS_E_NO_CERTIFICATE_FOUND) {
	} else if (certstat & GNUTLS_CERT_CORRUPTED) {
		bad_certificate(scs, "server's certificate is corrupt.\n");
	} else if (certstat & GNUTLS_CERT_REVOKED) {
		bad_certificate(scs, "server's certificate has been revoked.\n");
	} else if (certstat & GNUTLS_CERT_INVALID) {
		bad_certificate(scs, "server's certificate is invalid.\n");
	} else if (certstat & GNUTLS_CERT_NOT_TRUSTED) {
		TDM(DEBUG_INFO, "server's certificate is not trusted.\n");
		TDM(DEBUG_INFO,
			"at the moment, wmbiff doesn't trust certificates.\n");
	}

	/* not checking for not-yet-valid certs... this would make sense
	   if we weren't just comparing to stored ones */
	cert_list = gnutls_certificate_get_peers(scs->state, &cert_list_size);

	if (gnutls_x509_extract_certificate_expiration_time(&cert_list[0]) <
		time(NULL)) {
		bad_certificate(scs, "server's certificate has expired.\n");
	} else
		if (gnutls_x509_extract_certificate_activation_time(&cert_list[0])
			> time(NULL)) {
		bad_certificate(scs, "server's certificate is not yet valid.\n");
	} else {
		TDM(DEBUG_INFO, "certificate passed time check.\n");
	}

	if (gnutls_x509_check_certificates_hostname
		(&cert_list[0], remote_hostname) == 0) {
		gnutls_DN dn;
		gnutls_x509_extract_certificate_dn(&cert_list[0], &dn);
		TDM(DEBUG_ERROR,
			"server's certificate (%s) does not match its hostname (%s).\n",
			dn.common_name, remote_hostname);
		bad_certificate(scs,
						"server's certificate does not match its hostname.\n");
	} else {
		if ((scs->pc)->debug >= DEBUG_INFO) {
			gnutls_DN dn;
			gnutls_x509_extract_certificate_dn(&cert_list[0], &dn);
			TDM(DEBUG_INFO,
				"server's certificate (%s) matched its hostname (%s).\n",
				dn.common_name, remote_hostname);
		}
	}

	TDM(DEBUG_INFO, "certificate check ok.\n");
	return (0);
}

struct connection_state *initialize_gnutls(int sd, char *name, Pop3 pc,
										   const char *remote_hostname)
{
	static int gnutls_initialized;
	int zok;
	struct connection_state *scs = malloc(sizeof(struct connection_state));

	scs->pc = pc;

	assert(sd >= 0);

	if (gnutls_initialized == 0) {
		assert(gnutls_global_init() == 0);
		gnutls_initialized = 1;
	}

	assert(gnutls_init(&scs->state, GNUTLS_CLIENT) == 0);
	{
		const int protocols[] = { GNUTLS_TLS1, GNUTLS_SSL3, 0 };
		const int ciphers[] =
			{ GNUTLS_CIPHER_RIJNDAEL_128_CBC, GNUTLS_CIPHER_3DES_CBC,
			GNUTLS_CIPHER_RIJNDAEL_256_CBC, GNUTLS_CIPHER_TWOFISH_128_CBC,
			GNUTLS_CIPHER_ARCFOUR, 0
		};
		const int compress[] = { GNUTLS_COMP_ZLIB, GNUTLS_COMP_NULL, 0 };
		const int key_exch[] = { GNUTLS_KX_RSA, GNUTLS_KX_DHE_DSS,
			GNUTLS_KX_DHE_RSA, 0
		};
		/* mutt with gnutls doesn't use kx_srp or kx_anon_dh */
		const int mac[] = { GNUTLS_MAC_SHA, GNUTLS_MAC_MD5, 0 };
		assert(gnutls_protocol_set_priority(scs->state, protocols) == 0);
		assert(gnutls_cipher_set_priority(scs->state, ciphers) == 0);
		assert(gnutls_compression_set_priority(scs->state, compress) == 0);
		assert(gnutls_kx_set_priority(scs->state, key_exch) == 0);
		assert(gnutls_mac_set_priority(scs->state, mac) == 0);
		/* no client private key */
		if (gnutls_certificate_allocate_sc(&scs->xcred) < 0) {
			DMA(DEBUG_ERROR, "gnutls memory error\n");
			exit(1);
		}

		/* certfile really isn't supported; this is just a start. */
		if (certificate_filename != NULL) {
			if (!exists(certificate_filename)) {
				DMA(DEBUG_ERROR,
					"Certificate file (certfile=) %s not found.\n",
					certificate_filename);
				exit(1);
			}
			zok = gnutls_certificate_set_x509_trust_file(scs->xcred,
														 (char *)
														 certificate_filename,
														 GNUTLS_X509_FMT_PEM);
			if (zok != 0) {
				DMA(DEBUG_ERROR,
					"GNUTLS did not like your certificate file %s.\n",
					certificate_filename);
				gnutls_perror(zok);
				exit(1);
			}
		}

		gnutls_cred_set(scs->state, GNUTLS_CRD_CERTIFICATE, scs->xcred);
		gnutls_transport_set_ptr(scs->state, sd);
		do {
			zok = gnutls_handshake(scs->state);
		} while (zok == GNUTLS_E_INTERRUPTED || zok == GNUTLS_E_AGAIN);

		tls_check_certificate(scs, remote_hostname);
	}

	if (zok < 0) {
		TDM(DEBUG_ERROR, "%s: Handshake failed\n", name);
		TDM(DEBUG_ERROR, "%s: This may be a problem in gnutls, "
			"which is under development\n", name);
		TDM(DEBUG_ERROR,
			"%s: Specifically, problems have been found where the extnValue \n"
			"  buffer in _gnutls_get_ext_type() in lib/x509_extensions.c is too small in\n"
			"  gnutls versions up to 0.2.3.  This copy of wmbiff was compiled with \n"
			"  gnutls version %s.\n", name, LIBGNUTLS_VERSION);
		gnutls_perror(zok);
		gnutls_deinit(scs->state);
		free(scs);
		return (NULL);
	} else {
		TDM(DEBUG_INFO, "%s: Handshake was completed\n", name);
		if (scs->pc->debug >= DEBUG_INFO)
			print_info(scs->state);
		scs->sd = sd;
		scs->name = name;
	}
	return (scs);
}

/* moved down here, to keep from interrupting the flow with
   verbose error crap */
void handle_gnutls_read_error(int readbytes, struct connection_state *scs)
{
	if (gnutls_error_is_fatal(readbytes) == 1) {
		TDM(DEBUG_ERROR,
			"%s: Received corrupted data(%d) - server has terminated the connection abnormally\n",
			scs->name, readbytes);
	} else {
		if (readbytes == GNUTLS_E_WARNING_ALERT_RECEIVED
			|| readbytes == GNUTLS_E_FATAL_ALERT_RECEIVED)
			TDM(DEBUG_ERROR, "* Received alert [%d]\n",
				gnutls_alert_get(scs->state));
		if (readbytes == GNUTLS_E_REHANDSHAKE)
			TDM(DEBUG_ERROR, "* Received HelloRequest message\n");
	}
	TDM(DEBUG_ERROR,
		"%s: gnutls error reading: %s\n",
		scs->name, gnutls_strerror(readbytes));
}

#else
/* declare stubs when tls isn't compiled in */
struct connection_state *
initialize_gnutls( /*@unused@ */ int sd __attribute__ ((unused)),
                   /*@unused@ */ char *name __attribute__ ((unused)),
                   /*@unused@ */ Pop3 pc __attribute__ ((unused)),
                   /*@unused@ */ const char *remote_hostname __attribute__ ((unused)))
{
	DM(pc, DEBUG_ERROR,
	   "FATAL: tried to initialize ssl when ssl wasn't compiled in.\n");
	exit(EXIT_FAILURE);
}
#endif

/* either way: */
struct connection_state *initialize_unencrypted(int sd,
												/*@only@ */ char *name,
												Pop3 pc)
{
	struct connection_state *ret = malloc(sizeof(struct connection_state));
	assert(sd >= 0);
	assert(ret != NULL);
	ret->sd = sd;
	ret->name = name;
	ret->state = NULL;
	ret->xcred = NULL;
	ret->pc = pc;
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
	ret->pc = NULL;
	return (ret);
}


int tlscomm_is_blacklisted(const struct connection_state *scs)
{
	return (scs != NULL && scs->sd == -1);
}

/* vim:set ts=4: */
/*
 * Local Variables:
 * tab-width: 4
 * c-indent-level: 4
 * c-basic-offset: 4
 * End:
 */
