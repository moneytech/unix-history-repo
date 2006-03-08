/*
 * WPA Supplicant / Configuration backend: text file
 * Copyright (c) 2003-2005, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 *
 * This file implements a configuration backend for text files. All the
 * configuration information is stored in a text file that uses a format
 * described in the sample configuration file, wpa_supplicant.conf.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "wpa.h"
#include "wpa_supplicant.h"
#include "config.h"
#include "base64.h"


static char * wpa_config_get_line(char *s, int size, FILE *stream, int *line)
{
	char *pos, *end, *sstart;

	while (fgets(s, size, stream)) {
		(*line)++;
		s[size - 1] = '\0';
		pos = s;

		while (*pos == ' ' || *pos == '\t' || *pos == '\r')
			pos++;
		if (*pos == '#' || *pos == '\n' || *pos == '\0' ||
		    *pos == '\r')
			continue;

		/* Remove # comments unless they are within a double quoted
		 * string. Remove trailing white space. */
		sstart = strchr(pos, '"');
		if (sstart)
			sstart = strrchr(sstart + 1, '"');
		if (!sstart)
			sstart = pos;
		end = strchr(sstart, '#');
		if (end)
			*end-- = '\0';
		else
			end = pos + strlen(pos) - 1;
		while (end > pos &&
		       (*end == '\n' || *end == ' ' || *end == '\t' ||
			*end == '\r')) {
			*end-- = '\0';
		}
		if (*pos == '\0')
			continue;

		return pos;
	}

	return NULL;
}


static struct wpa_ssid * wpa_config_read_network(FILE *f, int *line, int id)
{
	struct wpa_ssid *ssid;
	int errors = 0, end = 0;
	char buf[256], *pos, *pos2;

	wpa_printf(MSG_MSGDUMP, "Line: %d - start of a new network block",
		   *line);
	ssid = (struct wpa_ssid *) malloc(sizeof(*ssid));
	if (ssid == NULL)
		return NULL;
	memset(ssid, 0, sizeof(*ssid));
	ssid->id = id;

	wpa_config_set_network_defaults(ssid);

	while ((pos = wpa_config_get_line(buf, sizeof(buf), f, line))) {
		if (strcmp(pos, "}") == 0) {
			end = 1;
			break;
		}

		pos2 = strchr(pos, '=');
		if (pos2 == NULL) {
			wpa_printf(MSG_ERROR, "Line %d: Invalid SSID line "
				   "'%s'.", *line, pos);
			errors++;
			continue;
		}

		*pos2++ = '\0';
		if (*pos2 == '"') {
			if (strchr(pos2 + 1, '"') == NULL) {
				wpa_printf(MSG_ERROR, "Line %d: invalid "
					   "quotation '%s'.", *line, pos2);
				errors++;
				continue;
			}
		}

		if (wpa_config_set(ssid, pos, pos2, *line) < 0)
			errors++;
	}

	if (!end) {
		wpa_printf(MSG_ERROR, "Line %d: network block was not "
			   "terminated properly.", *line);
		errors++;
	}

	if (ssid->passphrase) {
		if (ssid->psk_set) {
			wpa_printf(MSG_ERROR, "Line %d: both PSK and "
				   "passphrase configured.", *line);
			errors++;
		}
		wpa_config_update_psk(ssid);
	}

	if ((ssid->key_mgmt & WPA_KEY_MGMT_PSK) && !ssid->psk_set) {
		wpa_printf(MSG_ERROR, "Line %d: WPA-PSK accepted for key "
			   "management, but no PSK configured.", *line);
		errors++;
	}

	if ((ssid->group_cipher & WPA_CIPHER_CCMP) &&
	    !(ssid->pairwise_cipher & WPA_CIPHER_CCMP)) {
		/* Group cipher cannot be stronger than the pairwise cipher. */
		wpa_printf(MSG_DEBUG, "Line %d: removed CCMP from group cipher"
			   " list since it was not allowed for pairwise "
			   "cipher", *line);
		ssid->group_cipher &= ~WPA_CIPHER_CCMP;
	}

	if (errors) {
		free(ssid);
		ssid = NULL;
	}

	return ssid;
}


static struct wpa_config_blob * wpa_config_read_blob(FILE *f, int *line,
						     const char *name)
{
	struct wpa_config_blob *blob;
	char buf[256], *pos;
	unsigned char *encoded = NULL, *nencoded;
	int end = 0;
	size_t encoded_len = 0, len;

	wpa_printf(MSG_MSGDUMP, "Line: %d - start of a new named blob '%s'",
		   *line, name);

	while ((pos = wpa_config_get_line(buf, sizeof(buf), f, line))) {
		if (strcmp(pos, "}") == 0) {
			end = 1;
			break;
		}

		len = strlen(pos);
		nencoded = realloc(encoded, encoded_len + len);
		if (nencoded == NULL) {
			wpa_printf(MSG_ERROR, "Line %d: not enough memory for "
				   "blob", *line);
			free(encoded);
			return NULL;
		}
		encoded = nencoded;
		memcpy(encoded + encoded_len, pos, len);
		encoded_len += len;
	}

	if (!end) {
		wpa_printf(MSG_ERROR, "Line %d: blob was not terminated "
			   "properly", *line);
		free(encoded);
		return NULL;
	}

	blob = malloc(sizeof(*blob));
	if (blob == NULL) {
		free(encoded);
		return NULL;
	}
	memset(blob, 0, sizeof(*blob));
	blob->name = strdup(name);
	blob->data = base64_decode(encoded, encoded_len, &blob->len);
	free(encoded);

	if (blob->name == NULL || blob->data == NULL) {
		wpa_config_free_blob(blob);
		return NULL;
	}

	return blob;
}


struct wpa_config * wpa_config_read(const char *name)
{
	FILE *f;
	char buf[256], *pos;
	int errors = 0, line = 0;
	struct wpa_ssid *ssid, *tail = NULL, *head = NULL;
	struct wpa_config *config;
	int id = 0, prio;

	config = wpa_config_alloc_empty(NULL, NULL);
	if (config == NULL)
		return NULL;
	wpa_printf(MSG_DEBUG, "Reading configuration file '%s'", name);
	f = fopen(name, "r");
	if (f == NULL) {
		free(config);
		return NULL;
	}

	while ((pos = wpa_config_get_line(buf, sizeof(buf), f, &line))) {
		if (strcmp(pos, "network={") == 0) {
			ssid = wpa_config_read_network(f, &line, id++);
			if (ssid == NULL) {
				wpa_printf(MSG_ERROR, "Line %d: failed to "
					   "parse network block.", line);
				errors++;
				continue;
			}
			if (head == NULL) {
				head = tail = ssid;
			} else {
				tail->next = ssid;
				tail = ssid;
			}
			if (wpa_config_add_prio_network(config, ssid)) {
				wpa_printf(MSG_ERROR, "Line %d: failed to add "
					   "network block to priority list.",
					   line);
				errors++;
				continue;
			}
		} else if (strncmp(pos, "blob-base64-", 12) == 0) {
			char *name = pos + 12, *name_end;
			struct wpa_config_blob *blob;

			name_end = strchr(name, '=');
			if (name_end == NULL) {
				wpa_printf(MSG_ERROR, "Line %d: no blob name "
					   "terminator", line);
				errors++;
				continue;
			}
			*name_end = '\0';

			blob = wpa_config_read_blob(f, &line, name);
			if (blob == NULL) {
				wpa_printf(MSG_ERROR, "Line %d: failed to read"
					   " blob %s", line, name);
				errors++;
				continue;
			}
			wpa_config_set_blob(config, blob);
#ifdef CONFIG_CTRL_IFACE
		} else if (strncmp(pos, "ctrl_interface=", 15) == 0) {
			free(config->ctrl_interface);
			config->ctrl_interface = strdup(pos + 15);
			wpa_printf(MSG_DEBUG, "ctrl_interface='%s'",
				   config->ctrl_interface);
#ifndef CONFIG_CTRL_IFACE_UDP
		} else if (strncmp(pos, "ctrl_interface_group=", 21) == 0) {
			struct group *grp;
			char *endp;
			const char *group = pos + 21;

			grp = getgrnam(group);
			if (grp) {
				config->ctrl_interface_gid = grp->gr_gid;
				config->ctrl_interface_gid_set = 1;
				wpa_printf(MSG_DEBUG, "ctrl_interface_group=%d"
					   " (from group name '%s')",
					   (int) config->ctrl_interface_gid,
					   group);
				continue;
			}

			/* Group name not found - try to parse this as gid */
			config->ctrl_interface_gid = strtol(group, &endp, 10);
			if (*group == '\0' || *endp != '\0') {
				wpa_printf(MSG_DEBUG, "Line %d: Invalid group "
					   "'%s'", line, group);
				errors++;
				continue;
			}
			config->ctrl_interface_gid_set = 1;
			wpa_printf(MSG_DEBUG, "ctrl_interface_group=%d",
				   (int) config->ctrl_interface_gid);
#endif /* CONFIG_CTRL_IFACE_UDP */
#endif /* CONFIG_CTRL_IFACE */
		} else if (strncmp(pos, "eapol_version=", 14) == 0) {
			config->eapol_version = atoi(pos + 14);
			if (config->eapol_version < 1 ||
			    config->eapol_version > 2) {
				wpa_printf(MSG_ERROR, "Line %d: Invalid EAPOL "
					   "version (%d): '%s'.",
					   line, config->eapol_version, pos);
				errors++;
				continue;
			}
			wpa_printf(MSG_DEBUG, "eapol_version=%d",
				   config->eapol_version);
		} else if (strncmp(pos, "ap_scan=", 8) == 0) {
			config->ap_scan = atoi(pos + 8);
			wpa_printf(MSG_DEBUG, "ap_scan=%d", config->ap_scan);
		} else if (strncmp(pos, "fast_reauth=", 12) == 0) {
			config->fast_reauth = atoi(pos + 12);
			wpa_printf(MSG_DEBUG, "fast_reauth=%d",
				   config->fast_reauth);
		} else if (strncmp(pos, "opensc_engine_path=", 19) == 0) {
			free(config->opensc_engine_path);
			config->opensc_engine_path = strdup(pos + 19);
			wpa_printf(MSG_DEBUG, "opensc_engine_path='%s'",
				   config->opensc_engine_path);
		} else if (strncmp(pos, "pkcs11_engine_path=", 19) == 0) {
			free(config->pkcs11_engine_path);
			config->pkcs11_engine_path = strdup(pos + 19);
			wpa_printf(MSG_DEBUG, "pkcs11_engine_path='%s'",
				   config->pkcs11_engine_path);
		} else if (strncmp(pos, "pkcs11_module_path=", 19) == 0) {
			free(config->pkcs11_module_path);
			config->pkcs11_module_path = strdup(pos + 19);
			wpa_printf(MSG_DEBUG, "pkcs11_module_path='%s'",
				   config->pkcs11_module_path);
		} else if (strncmp(pos, "driver_param=", 13) == 0) {
			free(config->driver_param);
			config->driver_param = strdup(pos + 13);
			wpa_printf(MSG_DEBUG, "driver_param='%s'",
				   config->driver_param);
		} else if (strncmp(pos, "dot11RSNAConfigPMKLifetime=", 27) ==
			   0) {
			config->dot11RSNAConfigPMKLifetime = atoi(pos + 27);
			wpa_printf(MSG_DEBUG, "dot11RSNAConfigPMKLifetime=%d",
				   config->dot11RSNAConfigPMKLifetime);
		} else if (strncmp(pos, "dot11RSNAConfigPMKReauthThreshold=",
				   34) ==
			   0) {
			config->dot11RSNAConfigPMKReauthThreshold =
				atoi(pos + 34);
			wpa_printf(MSG_DEBUG,
				   "dot11RSNAConfigPMKReauthThreshold=%d",
				   config->dot11RSNAConfigPMKReauthThreshold);
		} else if (strncmp(pos, "dot11RSNAConfigSATimeout=", 25) ==
			   0) {
			config->dot11RSNAConfigSATimeout = atoi(pos + 25);
			wpa_printf(MSG_DEBUG, "dot11RSNAConfigSATimeout=%d",
				   config->dot11RSNAConfigSATimeout);
		} else if (strncmp(pos, "update_config=", 14) == 0) {
			config->update_config = atoi(pos + 14);
			wpa_printf(MSG_DEBUG, "update_config=%d",
				   config->update_config);
		} else {
			wpa_printf(MSG_ERROR, "Line %d: Invalid configuration "
				   "line '%s'.", line, pos);
			errors++;
			continue;
		}
	}

	fclose(f);

	config->ssid = head;
	for (prio = 0; prio < config->num_prio; prio++) {
		ssid = config->pssid[prio];
		wpa_printf(MSG_DEBUG, "Priority group %d",
			   ssid->priority);
		while (ssid) {
			wpa_printf(MSG_DEBUG, "   id=%d ssid='%s'",
				   ssid->id,
				   wpa_ssid_txt(ssid->ssid, ssid->ssid_len));
			ssid = ssid->pnext;
		}
	}
	if (errors) {
		wpa_config_free(config);
		config = NULL;
		head = NULL;
	}

	return config;
}


static void write_str(FILE *f, const char *field, struct wpa_ssid *ssid)
{
	char *value = wpa_config_get(ssid, field);
	if (value == NULL)
		return;
	fprintf(f, "\t%s=%s\n", field, value);
	free(value);
}


static void write_int(FILE *f, const char *field, int value, int def)
{
	if (value == def)
		return;
	fprintf(f, "\t%s=%d\n", field, value);
}


static void write_bssid(FILE *f, struct wpa_ssid *ssid)
{
	char *value = wpa_config_get(ssid, "bssid");
	if (value == NULL)
		return;
	fprintf(f, "\tbssid=%s\n", value);
	free(value);
}


static void write_psk(FILE *f, struct wpa_ssid *ssid)
{
	char *value = wpa_config_get(ssid, "psk");
	if (value == NULL)
		return;
	fprintf(f, "\tpsk=%s\n", value);
	free(value);
}


static void write_proto(FILE *f, struct wpa_ssid *ssid)
{
	char *value;

	if (ssid->proto == DEFAULT_PROTO)
		return;

	value = wpa_config_get(ssid, "proto");
	if (value == NULL)
		return;
	if (value[0])
		fprintf(f, "\tproto=%s\n", value);
	free(value);
}


static void write_key_mgmt(FILE *f, struct wpa_ssid *ssid)
{
	char *value;

	if (ssid->key_mgmt == DEFAULT_KEY_MGMT)
		return;

	value = wpa_config_get(ssid, "key_mgmt");
	if (value == NULL)
		return;
	if (value[0])
		fprintf(f, "\tkey_mgmt=%s\n", value);
	free(value);
}


static void write_pairwise(FILE *f, struct wpa_ssid *ssid)
{
	char *value;

	if (ssid->pairwise_cipher == DEFAULT_PAIRWISE)
		return;

	value = wpa_config_get(ssid, "pairwise");
	if (value == NULL)
		return;
	if (value[0])
		fprintf(f, "\tpairwise=%s\n", value);
	free(value);
}


static void write_group(FILE *f, struct wpa_ssid *ssid)
{
	char *value;

	if (ssid->group_cipher == DEFAULT_GROUP)
		return;

	value = wpa_config_get(ssid, "group");
	if (value == NULL)
		return;
	if (value[0])
		fprintf(f, "\tgroup=%s\n", value);
	free(value);
}


static void write_auth_alg(FILE *f, struct wpa_ssid *ssid)
{
	char *value;

	if (ssid->auth_alg == 0)
		return;

	value = wpa_config_get(ssid, "auth_alg");
	if (value == NULL)
		return;
	if (value[0])
		fprintf(f, "\tauth_alg=%s\n", value);
	free(value);
}


static void write_eap(FILE *f, struct wpa_ssid *ssid)
{
	char *value;

	value = wpa_config_get(ssid, "eap");
	if (value == NULL)
		return;

	if (value[0])
		fprintf(f, "\teap=%s\n", value);
	free(value);
}


static void write_wep_key(FILE *f, int idx, struct wpa_ssid *ssid)
{
	char field[20], *value;

	snprintf(field, sizeof(field), "wep_key%d", idx);
	value = wpa_config_get(ssid, field);
	if (value) {
		fprintf(f, "\t%s=%s\n", field, value);
		free(value);
	}
}


static void wpa_config_write_network(FILE *f, struct wpa_ssid *ssid)
{
	int i;

#define STR(t) write_str(f, #t, ssid)
#define INT(t) write_int(f, #t, ssid->t, 0)
#define INT_DEF(t, def) write_int(f, #t, ssid->t, def)

	STR(ssid);
	INT(scan_ssid);
	write_bssid(f, ssid);
	write_psk(f, ssid);
	write_proto(f, ssid);
	write_key_mgmt(f, ssid);
	write_pairwise(f, ssid);
	write_group(f, ssid);
	write_auth_alg(f, ssid);
	write_eap(f, ssid);
	STR(identity);
	STR(anonymous_identity);
	STR(eappsk);
	STR(nai);
	STR(password);
	STR(ca_cert);
	STR(client_cert);
	STR(private_key);
	STR(private_key_passwd);
	STR(dh_file);
	STR(subject_match);
	STR(altsubject_match);
	STR(ca_cert2);
	STR(client_cert2);
	STR(private_key2);
	STR(private_key2_passwd);
	STR(dh_file2);
	STR(subject_match2);
	STR(altsubject_match2);
	STR(phase1);
	STR(phase2);
	STR(pcsc);
	STR(pin);
	STR(engine_id);
	STR(key_id);
	INT(engine);
	INT_DEF(eapol_flags, DEFAULT_EAPOL_FLAGS);
	for (i = 0; i < 4; i++)
		write_wep_key(f, i, ssid);
	INT(wep_tx_keyidx);
	INT(priority);
	INT_DEF(eap_workaround, DEFAULT_EAP_WORKAROUND);
	STR(pac_file);
	INT(mode);
	INT(proactive_key_caching);
	INT(disabled);

#undef STR
#undef INT
#undef INT_DEF
}


static int wpa_config_write_blob(FILE *f, struct wpa_config_blob *blob)
{
	unsigned char *encoded;

	encoded = base64_encode(blob->data, blob->len, NULL);
	if (encoded == NULL)
		return -1;

	fprintf(f, "\nblob-base64-%s={\n%s}\n", blob->name, encoded);
	free(encoded);
	return 0;
}


int wpa_config_write(const char *name, struct wpa_config *config)
{
	FILE *f;
	struct wpa_ssid *ssid;
	struct wpa_config_blob *blob;
	int ret = 0;

	wpa_printf(MSG_DEBUG, "Writing configuration file '%s'", name);


	f = fopen(name, "w");
	if (f == NULL) {
		wpa_printf(MSG_DEBUG, "Failed to open '%s' for writing", name);
		return -1;
	}

#ifdef CONFIG_CTRL_IFACE
	if (config->ctrl_interface)
		fprintf(f, "ctrl_interface=%s\n", config->ctrl_interface);
#ifndef CONFIG_CTRL_IFACE_UDP
	if (config->ctrl_interface_gid_set) {
		fprintf(f, "ctrl_interface_group=%d\n",
			(int) config->ctrl_interface_gid);
	}
#endif /* CONFIG_CTRL_IFACE_UDP */
#endif /* CONFIG_CTRL_IFACE */
	if (config->eapol_version != DEFAULT_EAPOL_VERSION)
		fprintf(f, "eapol_version=%d\n", config->eapol_version);
	if (config->ap_scan != DEFAULT_AP_SCAN)
		fprintf(f, "ap_scan=%d\n", config->ap_scan);
	if (config->fast_reauth != DEFAULT_FAST_REAUTH)
		fprintf(f, "fast_reauth=%d\n", config->fast_reauth);
	if (config->opensc_engine_path)
		fprintf(f, "opensc_engine_path=%s\n",
			config->opensc_engine_path);
	if (config->pkcs11_engine_path)
		fprintf(f, "pkcs11_engine_path=%s\n",
			config->pkcs11_engine_path);
	if (config->pkcs11_module_path)
		fprintf(f, "pkcs11_module_path=%s\n",
			config->pkcs11_module_path);
	if (config->driver_param)
		fprintf(f, "driver_param=%s\n", config->driver_param);
	if (config->dot11RSNAConfigPMKLifetime)
		fprintf(f, "dot11RSNAConfigPMKLifetime=%d\n",
			config->dot11RSNAConfigPMKLifetime);
	if (config->dot11RSNAConfigPMKReauthThreshold)
		fprintf(f, "dot11RSNAConfigPMKReauthThreshold=%d\n",
			config->dot11RSNAConfigPMKReauthThreshold);
	if (config->dot11RSNAConfigSATimeout)
		fprintf(f, "dot11RSNAConfigSATimeout=%d\n",
			config->dot11RSNAConfigSATimeout);
	if (config->update_config)
		fprintf(f, "update_config=%d\n", config->update_config);

	for (ssid = config->ssid; ssid; ssid = ssid->next) {
		fprintf(f, "\nnetwork={\n");
		wpa_config_write_network(f, ssid);
		fprintf(f, "}\n");
	}

	for (blob = config->blobs; blob; blob = blob->next) {
		ret = wpa_config_write_blob(f, blob);
		if (ret)
			break;
	}

	fclose(f);

	wpa_printf(MSG_DEBUG, "Configuration file '%s' written %ssuccessfully",
		   name, ret ? "un" : "");
	return ret;
}
