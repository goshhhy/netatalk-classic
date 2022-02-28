/*
 * Copyright (c) 1997 Adrian Sun (asun@zoology.washington.edu)
 * All Rights Reserved.  See COPYRIGHT.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <atalk/logger.h>
#include <atalk/util.h>
#include <atalk/atp.h>
#include <atalk/asp.h>
#include <atalk/nbp.h>
#include <atalk/afp.h>
#include <atalk/compat.h>
#include <atalk/server_child.h>

#ifdef HAVE_LDAP
#include <atalk/ldapconfig.h>
#endif

#include <atalk/globals.h>
#include "afp_config.h"
#include "uam_auth.h"
#include "status.h"
#include "volume.h"

#define LINESIZE 1024

/* get rid of unneeded configurations. i use reference counts to deal
 * w/ multiple configs sharing the same afp_options. oh, to dream of
 * garbage collection ... */
void configfree(AFPConfig * configs, const AFPConfig * config)
{
	AFPConfig *p, *q;

	for (p = configs; p; p = q) {
		q = p->next;
		if (p == config)
			continue;

		/* do a little reference counting */
		if (--(*p->optcount) < 1) {
			afp_options_free(&p->obj.options, p->defoptions);
			free(p->optcount);
		}

		free(p->obj.Obj);
		free(p->obj.Type);
		free(p->obj.Zone);
		atp_close(((ASP) p->obj.handle)->asp_atp);
		free(p->obj.handle);

		free(p);
	}

	/* the master loaded the volumes for zeroconf, get rid of that */
	unload_volumes_and_extmap();
}


static void asp_cleanup(const AFPConfig * config)
{
	/* we need to stop tickle handler */
	asp_stop_tickle();
	nbp_unrgstr(config->obj.Obj, config->obj.Type, config->obj.Zone,
		    &config->obj.options.ddpaddr);
}

/* these two are almost identical. it should be possible to collapse them
 * into one with minimal junk. */
static int asp_start(AFPConfig * config, AFPConfig * configs,
		     server_child * server_children)
{
	ASP asp;

	if (!(asp = asp_getsession(config->obj.handle, server_children,
				   config->obj.options.tickleval))) {
		LOG(log_error, logtype_afpd, "main: asp_getsession: %s",
		    strerror(errno));
		exit(EXITERR_CLNT);
	}

	if (asp->child) {
		configfree(configs, config);	/* free a bunch of stuff */
		afp_over_asp(&config->obj);
		exit(0);
	}

	return 0;
}

static AFPConfig *ASPConfigInit(const struct afp_options *options,
				unsigned char *refcount)
{
	AFPConfig *config;
	ATP atp;
	ASP asp;
	char *Obj, *Type = "AFPServer", *Zone = "*";
	char *convname = NULL;

	if ((config = (AFPConfig *) calloc(1, sizeof(AFPConfig))) == NULL)
		return NULL;

	if ((atp = atp_open(ATADDR_ANYPORT, &options->ddpaddr)) == NULL) {
		LOG(log_error, logtype_afpd, "main: atp_open: %s",
		    strerror(errno));
		free(config);
		return NULL;
	}

	if ((asp = asp_init(atp)) == NULL) {
		LOG(log_error, logtype_afpd, "main: asp_init: %s",
		    strerror(errno));
		atp_close(atp);
		free(config);
		return NULL;
	}

	/* register asp server */
	Obj = (char *) options->hostname;
	if (options->server
	    && (size_t) -1 ==
	    (convert_string_allocate
	     (options->unixcharset, options->maccharset, options->server,
	      strlen(options->server), &convname))) {
		if ((convname = strdup(options->server)) == NULL) {
			LOG(log_error, logtype_afpd, "malloc: %s",
			    strerror(errno));
			goto serv_free_return;
		}
	}

	if (nbp_name(convname, &Obj, &Type, &Zone)) {
		LOG(log_error, logtype_afpd, "main: can't parse %s",
		    options->server);
		goto serv_free_return;
	}
	if (convname)
		free(convname);

	/* dup Obj, Type and Zone as they get assigned to a single internal
	 * buffer by nbp_name */
	if ((config->obj.Obj = strdup(Obj)) == NULL)
		goto serv_free_return;

	if ((config->obj.Type = strdup(Type)) == NULL) {
		free(config->obj.Obj);
		goto serv_free_return;
	}

	if ((config->obj.Zone = strdup(Zone)) == NULL) {
		free(config->obj.Obj);
		free(config->obj.Type);
		goto serv_free_return;
	}

	/* make sure we're not registered */
	nbp_unrgstr(Obj, Type, Zone, &options->ddpaddr);
	if (nbp_rgstr(atp_sockaddr(atp), Obj, Type, Zone) < 0) {
		LOG(log_error, logtype_afpd, "Can't register %s:%s@%s",
		    Obj, Type, Zone);
		free(config->obj.Obj);
		free(config->obj.Type);
		free(config->obj.Zone);
		goto serv_free_return;
	}

	LOG(log_info, logtype_afpd, "%s:%s@%s started on %u.%u:%u (%s)",
	    Obj, Type, Zone, ntohs(atp_sockaddr(atp)->sat_addr.s_net),
	    atp_sockaddr(atp)->sat_addr.s_node,
	    atp_sockaddr(atp)->sat_port, VERSION);

	config->fd = atp_fileno(atp);
	config->obj.handle = asp;
	config->obj.config = config;
	config->obj.proto = AFPPROTO_ASP;

	memcpy(&config->obj.options, options, sizeof(struct afp_options));
	config->optcount = refcount;
	(*refcount)++;

	config->server_start = (void *) asp_start;
	config->server_cleanup = (void *) asp_cleanup;

	return config;

      serv_free_return:
	asp_close(asp);
	free(config);
	return NULL;
}


/* allocate server configurations */
static AFPConfig *AFPConfigInit(struct afp_options *options,
				const struct afp_options *defoptions)
{
	AFPConfig *config = NULL, *next = NULL;
	unsigned char *refcount;

	if ((refcount = (unsigned char *)
	     calloc(1, sizeof(unsigned char))) == NULL) {
		LOG(log_error, logtype_afpd,
		    "AFPConfigInit: calloc(refcount): %s",
		    strerror(errno));
		return NULL;
	}

	/* handle asp transports */
	config = ASPConfigInit(options, refcount);
	if (config == NULL) {
		LOG(log_error, logtype_afpd,
		    "AFPConfigInit: ASPConfigInit() returned NULL");
		return NULL;
	}

	/* set signature */
	set_signature(options);

	/* load in all the authentication modules. we can load the same
	   things multiple times if necessary. however, loading different
	   things with the same names will cause complaints. by not loading
	   in any uams with proxies, we prevent ddp connections from succeeding.
	 */
	auth_load(options->uampath, options->uamlist);

	status_init(config, options);

	/* attach dsi config to tail of asp config - CK investigate */
	if (config) {
		config->next = next;
		return config;
	}

	return next;
}

/* fill in the appropriate bits for each interface */
AFPConfig *configinit(struct afp_options *cmdline)
{
	FILE *fp;
	char buf[LINESIZE + 1], *p, have_option = 0;
	size_t len;
	struct afp_options options;
	AFPConfig *config = NULL, *first = NULL;

	/* if config file doesn't exist, load defaults */
	if ((fp = fopen(cmdline->configfile, "r")) == NULL) {
		LOG(log_debug, logtype_afpd,
		    "ConfigFile %s not found, assuming defaults",
		    cmdline->configfile);
		return AFPConfigInit(cmdline, cmdline);
	}

	/* scan in the configuration file */
	len = 0;
	while (!feof(fp)) {
		if (!fgets(&buf[len], LINESIZE - len, fp)
		    || buf[len] == '#')
			continue;
		len = strlen(buf);
		if (len >= 2 && buf[len - 2] == '\\') {
			len -= 2;
			continue;
		} else
			len = 0;

		/* a little pre-processing to get rid of spaces and end-of-lines */
		p = buf;
		while (p && isspace(*p))
			p++;
		if (!p || (*p == '\0'))
			continue;

		have_option = 1;

		memcpy(&options, cmdline, sizeof(options));
		if (!afp_options_parseline(p, &options))
			continue;

		/* AFPConfigInit can return two linked configs due to DSI and ASP */
		if (!first) {
			if ((first = AFPConfigInit(&options, cmdline)))
				config = first->next ? first->next : first;
		} else
		    if ((config->next =
			 AFPConfigInit(&options, cmdline))) {
			config =
			    config->next->next ? config->
			    next->next : config->next;
		}
	}

#ifdef HAVE_LDAP
	/* Parse afp_ldap.conf */
	acl_ldap_readconfig(_PATH_ACL_LDAPCONF);
#endif				/* HAVE_LDAP */

	LOG(log_debug, logtype_afpd, "Finished parsing Config File");
	fclose(fp);

	if (!have_option)
		first = AFPConfigInit(cmdline, cmdline);

	return first;
}
