/*
 * And thus spoke RPCGEN:
 *    Please do not edit this file.
 *    It was generated using rpcgen.
 *
 * And thus replied Lpd@NannyMUD:
 *    Who cares? :-) /Peter Eriksson <pen@signum.se>
 *
 *
 * Modification history:
 *    940716 pen@signum.se      Change "ypreq_key" to "ypreq_nokey" for FIRST.
 *
 * $FreeBSD$
 */

#ifndef _YP_H_RPCGEN
#define _YP_H_RPCGEN

#include <rpc/rpc.h>
#ifdef NEED_SVCSOC_H
#include <rpc/svc_soc.h>
#endif

#define YPMAXRECORD 1024
#define YPMAXDOMAIN 64
#define YPMAXMAP 64
#define YPMAXPEER 64

enum ypstat {
	YP_TRUE = 1,
	YP_NOMORE = 2,
	YP_FALSE = 0,
	YP_NOMAP = -1,
	YP_NODOM = -2,
	YP_NOKEY = -3,
	YP_BADOP = -4,
	YP_BADDB = -5,
	YP_YPERR = -6,
	YP_BADARGS = -7,
	YP_VERS = -8
};
typedef enum ypstat ypstat;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypstat(XDR *, ypstat*);
#elif __STDC__
extern  bool_t __xdr_ypstat(XDR *, ypstat*);
#else /* Old Style C */
bool_t __xdr_ypstat();
#endif /* Old Style C */


enum ypxfrstat {
	YPXFR_SUCC = 1,
	YPXFR_AGE = 2,
	YPXFR_NOMAP = -1,
	YPXFR_NODOM = -2,
	YPXFR_RSRC = -3,
	YPXFR_RPC = -4,
	YPXFR_MADDR = -5,
	YPXFR_YPERR = -6,
	YPXFR_BADARGS = -7,
	YPXFR_DBM = -8,
	YPXFR_FILE = -9,
	YPXFR_SKEW = -10,
	YPXFR_CLEAR = -11,
	YPXFR_FORCE = -12,
	YPXFR_XFRERR = -13,
	YPXFR_REFUSED = -14
};
typedef enum ypxfrstat ypxfrstat;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypxfrstat(XDR *, ypxfrstat*);
#elif __STDC__
extern  bool_t __xdr_ypxfrstat(XDR *, ypxfrstat*);
#else /* Old Style C */
bool_t __xdr_ypxfrstat();
#endif /* Old Style C */


typedef char *domainname;
#ifdef __cplusplus
extern "C" bool_t __xdr_domainname(XDR *, domainname*);
#elif __STDC__
extern  bool_t __xdr_domainname(XDR *, domainname*);
#else /* Old Style C */
bool_t __xdr_domainname();
#endif /* Old Style C */


typedef char *mapname;
#ifdef __cplusplus
extern "C" bool_t __xdr_mapname(XDR *, mapname*);
#elif __STDC__
extern  bool_t __xdr_mapname(XDR *, mapname*);
#else /* Old Style C */
bool_t __xdr_mapname();
#endif /* Old Style C */


typedef char *peername;
#ifdef __cplusplus
extern "C" bool_t __xdr_peername(XDR *, peername*);
#elif __STDC__
extern  bool_t __xdr_peername(XDR *, peername*);
#else /* Old Style C */
bool_t __xdr_peername();
#endif /* Old Style C */


typedef struct {
	u_int keydat_len;
	char *keydat_val;
} keydat;
#ifdef __cplusplus
extern "C" bool_t __xdr_keydat(XDR *, keydat*);
#elif __STDC__
extern  bool_t __xdr_keydat(XDR *, keydat*);
#else /* Old Style C */
bool_t __xdr_keydat();
#endif /* Old Style C */


typedef struct {
	u_int valdat_len;
	char *valdat_val;
} valdat;
#ifdef __cplusplus
extern "C" bool_t __xdr_valdat(XDR *, valdat*);
#elif __STDC__
extern  bool_t __xdr_valdat(XDR *, valdat*);
#else /* Old Style C */
bool_t __xdr_valdat();
#endif /* Old Style C */


struct ypmap_parms {
	domainname domain;
	mapname map;
	u_int ordernum;
	peername peer;
};
typedef struct ypmap_parms ypmap_parms;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypmap_parms(XDR *, ypmap_parms*);
#elif __STDC__
extern  bool_t __xdr_ypmap_parms(XDR *, ypmap_parms*);
#else /* Old Style C */
bool_t __xdr_ypmap_parms();
#endif /* Old Style C */


struct ypreq_key {
	domainname domain;
	mapname map;
	keydat key;
};
typedef struct ypreq_key ypreq_key;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypreq_key(XDR *, ypreq_key*);
#elif __STDC__
extern  bool_t __xdr_ypreq_key(XDR *, ypreq_key*);
#else /* Old Style C */
bool_t __xdr_ypreq_key();
#endif /* Old Style C */


struct ypreq_nokey {
	domainname domain;
	mapname map;
};
typedef struct ypreq_nokey ypreq_nokey;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypreq_nokey(XDR *, ypreq_nokey*);
#elif __STDC__
extern  bool_t __xdr_ypreq_nokey(XDR *, ypreq_nokey*);
#else /* Old Style C */
bool_t __xdr_ypreq_nokey();
#endif /* Old Style C */


struct ypreq_xfr {
	ypmap_parms map_parms;
	u_int transid;
	u_int prog;
	u_int port;
};
typedef struct ypreq_xfr ypreq_xfr;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypreq_xfr(XDR *, ypreq_xfr*);
#elif __STDC__
extern  bool_t __xdr_ypreq_xfr(XDR *, ypreq_xfr*);
#else /* Old Style C */
bool_t __xdr_ypreq_xfr();
#endif /* Old Style C */


struct ypresp_val {
	ypstat stat;
	valdat val;
};
typedef struct ypresp_val ypresp_val;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypresp_val(XDR *, ypresp_val*);
#elif __STDC__
extern  bool_t __xdr_ypresp_val(XDR *, ypresp_val*);
#else /* Old Style C */
bool_t __xdr_ypresp_val();
#endif /* Old Style C */


struct ypresp_key_val {
	ypstat stat;
	keydat key;
	valdat val;
};
typedef struct ypresp_key_val ypresp_key_val;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypresp_key_val(XDR *, ypresp_key_val*);
#elif __STDC__
extern  bool_t __xdr_ypresp_key_val(XDR *, ypresp_key_val*);
#else /* Old Style C */
bool_t __xdr_ypresp_key_val();
#endif /* Old Style C */


struct ypresp_master {
	ypstat stat;
	peername peer;
};
typedef struct ypresp_master ypresp_master;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypresp_master(XDR *, ypresp_master*);
#elif __STDC__
extern  bool_t __xdr_ypresp_master(XDR *, ypresp_master*);
#else /* Old Style C */
bool_t __xdr_ypresp_master();
#endif /* Old Style C */


struct ypresp_order {
	ypstat stat;
	u_int ordernum;
};
typedef struct ypresp_order ypresp_order;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypresp_order(XDR *, ypresp_order*);
#elif __STDC__
extern  bool_t __xdr_ypresp_order(XDR *, ypresp_order*);
#else /* Old Style C */
bool_t __xdr_ypresp_order();
#endif /* Old Style C */


typedef struct
{
    struct
    {
	int (*encode)(ypresp_key_val *val, void *data);
	int (*close)(void *data);
    } u;
    void *data;
} __xdr_ypall_cb_t;


struct ypresp_all {
	bool_t more;
	union {
		ypresp_key_val val;
	} ypresp_all_u;
};
typedef struct ypresp_all ypresp_all;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypresp_all(XDR *, ypresp_all*);
#elif __STDC__
extern  bool_t __xdr_ypresp_all(XDR *, ypresp_all*);
#else /* Old Style C */
bool_t __xdr_ypresp_all();
#endif /* Old Style C */


struct ypresp_xfr {
	u_int transid;
	ypxfrstat xfrstat;
};
typedef struct ypresp_xfr ypresp_xfr;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypresp_xfr(XDR *, ypresp_xfr*);
#elif __STDC__
extern  bool_t __xdr_ypresp_xfr(XDR *, ypresp_xfr*);
#else /* Old Style C */
bool_t __xdr_ypresp_xfr();
#endif /* Old Style C */


struct ypmaplist {
	mapname map;
	struct ypmaplist *next;
};
typedef struct ypmaplist ypmaplist;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypmaplist(XDR *, ypmaplist*);
#elif __STDC__
extern  bool_t __xdr_ypmaplist(XDR *, ypmaplist*);
#else /* Old Style C */
bool_t __xdr_ypmaplist();
#endif /* Old Style C */


struct ypresp_maplist {
	ypstat stat;
	ypmaplist *maps;
};
typedef struct ypresp_maplist ypresp_maplist;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypresp_maplist(XDR *, ypresp_maplist*);
#elif __STDC__
extern  bool_t __xdr_ypresp_maplist(XDR *, ypresp_maplist*);
#else /* Old Style C */
bool_t __xdr_ypresp_maplist();
#endif /* Old Style C */


enum yppush_status {
	YPPUSH_SUCC = 1,
	YPPUSH_AGE = 2,
	YPPUSH_NOMAP = -1,
	YPPUSH_NODOM = -2,
	YPPUSH_RSRC = -3,
	YPPUSH_RPC = -4,
	YPPUSH_MADDR = -5,
	YPPUSH_YPERR = -6,
	YPPUSH_BADARGS = -7,
	YPPUSH_DBM = -8,
	YPPUSH_FILE = -9,
	YPPUSH_SKEW = -10,
	YPPUSH_CLEAR = -11,
	YPPUSH_FORCE = -12,
	YPPUSH_XFRERR = -13,
	YPPUSH_REFUSED = -14
};
typedef enum yppush_status yppush_status;
#ifdef __cplusplus
extern "C" bool_t __xdr_yppush_status(XDR *, yppush_status*);
#elif __STDC__
extern  bool_t __xdr_yppush_status(XDR *, yppush_status*);
#else /* Old Style C */
bool_t __xdr_yppush_status();
#endif /* Old Style C */


struct yppushresp_xfr {
	u_int transid;
	yppush_status status;
};
typedef struct yppushresp_xfr yppushresp_xfr;
#ifdef __cplusplus
extern "C" bool_t __xdr_yppushresp_xfr(XDR *, yppushresp_xfr*);
#elif __STDC__
extern  bool_t __xdr_yppushresp_xfr(XDR *, yppushresp_xfr*);
#else /* Old Style C */
bool_t __xdr_yppushresp_xfr();
#endif /* Old Style C */


enum ypbind_resptype {
	YPBIND_SUCC_VAL = 1,
	YPBIND_FAIL_VAL = 2
};
typedef enum ypbind_resptype ypbind_resptype;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypbind_resptype(XDR *, ypbind_resptype*);
#elif __STDC__
extern  bool_t __xdr_ypbind_resptype(XDR *, ypbind_resptype*);
#else /* Old Style C */
bool_t __xdr_ypbind_resptype();
#endif /* Old Style C */


struct ypbind_binding {
	char ypbind_binding_addr[4];
	char ypbind_binding_port[2];
};
typedef struct ypbind_binding ypbind_binding;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypbind_binding(XDR *, ypbind_binding*);
#elif __STDC__
extern  bool_t __xdr_ypbind_binding(XDR *, ypbind_binding*);
#else /* Old Style C */
bool_t __xdr_ypbind_binding();
#endif /* Old Style C */


struct ypbind_resp {
	ypbind_resptype ypbind_status;
	union {
		u_int ypbind_error;
		ypbind_binding ypbind_bindinfo;
	} ypbind_resp_u;
};
typedef struct ypbind_resp ypbind_resp;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypbind_resp(XDR *, ypbind_resp*);
#elif __STDC__
extern  bool_t __xdr_ypbind_resp(XDR *, ypbind_resp*);
#else /* Old Style C */
bool_t __xdr_ypbind_resp();
#endif /* Old Style C */

#define YPBIND_ERR_ERR 1
#define YPBIND_ERR_NOSERV 2
#define YPBIND_ERR_RESC 3

struct ypbind_setdom {
	domainname ypsetdom_domain;
	ypbind_binding ypsetdom_binding;
	u_int ypsetdom_vers;
};
typedef struct ypbind_setdom ypbind_setdom;
#ifdef __cplusplus
extern "C" bool_t __xdr_ypbind_setdom(XDR *, ypbind_setdom*);
#elif __STDC__
extern  bool_t __xdr_ypbind_setdom(XDR *, ypbind_setdom*);
#else /* Old Style C */
bool_t __xdr_ypbind_setdom();
#endif /* Old Style C */


#define YPPROG ((u_long)100004)
#define YPVERS ((u_long)2)

#ifdef __cplusplus
#define YPPROC_NULL ((u_long)0)
extern "C" void * ypproc_null_2(void *, CLIENT *);
extern "C" void * ypproc_null_2_svc(void *, struct svc_req *);
#define YPPROC_DOMAIN ((u_long)1)
extern "C" bool_t * ypproc_domain_2(domainname *, CLIENT *);
extern "C" bool_t * ypproc_domain_2_svc(domainname *, struct svc_req *);
#define YPPROC_DOMAIN_NONACK ((u_long)2)
extern "C" bool_t * ypproc_domain_nonack_2(domainname *, CLIENT *);
extern "C" bool_t * ypproc_domain_nonack_2_svc(domainname *, struct svc_req *);
#define YPPROC_MATCH ((u_long)3)
extern "C" ypresp_val * ypproc_match_2(ypreq_key *, CLIENT *);
extern "C" ypresp_val * ypproc_match_2_svc(ypreq_key *, struct svc_req *);
#define YPPROC_FIRST ((u_long)4)
extern "C" ypresp_key_val * ypproc_first_2(ypreq_nokey *, CLIENT *);
extern "C" ypresp_key_val * ypproc_first_2_svc(ypreq_nokey *, struct svc_req *);
#define YPPROC_NEXT ((u_long)5)
extern "C" ypresp_key_val * ypproc_next_2(ypreq_key *, CLIENT *);
extern "C" ypresp_key_val * ypproc_next_2_svc(ypreq_key *, struct svc_req *);
#define YPPROC_XFR ((u_long)6)
extern "C" ypresp_xfr * ypproc_xfr_2(ypreq_xfr *, CLIENT *);
extern "C" ypresp_xfr * ypproc_xfr_2_svc(ypreq_xfr *, struct svc_req *);
#define YPPROC_CLEAR ((u_long)7)
extern "C" void * ypproc_clear_2(void *, CLIENT *);
extern "C" void * ypproc_clear_2_svc(void *, struct svc_req *);
#define YPPROC_ALL ((u_long)8)
extern "C" ypresp_all * ypproc_all_2(ypreq_nokey *, CLIENT *);
extern "C" ypresp_all * ypproc_all_2_svc(ypreq_nokey *, struct svc_req *);
#define YPPROC_MASTER ((u_long)9)
extern "C" ypresp_master * ypproc_master_2(ypreq_nokey *, CLIENT *);
extern "C" ypresp_master * ypproc_master_2_svc(ypreq_nokey *, struct svc_req *);
#define YPPROC_ORDER ((u_long)10)
extern "C" ypresp_order * ypproc_order_2(ypreq_nokey *, CLIENT *);
extern "C" ypresp_order * ypproc_order_2_svc(ypreq_nokey *, struct svc_req *);
#define YPPROC_MAPLIST ((u_long)11)
extern "C" ypresp_maplist * ypproc_maplist_2(domainname *, CLIENT *);
extern "C" ypresp_maplist * ypproc_maplist_2_svc(domainname *, struct svc_req *);

#elif __STDC__
#define YPPROC_NULL ((u_long)0)
extern  void * ypproc_null_2(void *, CLIENT *);
extern  void * ypproc_null_2_svc(void *, struct svc_req *);
#define YPPROC_DOMAIN ((u_long)1)
extern  bool_t * ypproc_domain_2(domainname *, CLIENT *);
extern  bool_t * ypproc_domain_2_svc(domainname *, struct svc_req *);
#define YPPROC_DOMAIN_NONACK ((u_long)2)
extern  bool_t * ypproc_domain_nonack_2(domainname *, CLIENT *);
extern  bool_t * ypproc_domain_nonack_2_svc(domainname *, struct svc_req *);
#define YPPROC_MATCH ((u_long)3)
extern  ypresp_val * ypproc_match_2(ypreq_key *, CLIENT *);
extern  ypresp_val * ypproc_match_2_svc(ypreq_key *, struct svc_req *);
#define YPPROC_FIRST ((u_long)4)
extern  ypresp_key_val * ypproc_first_2(ypreq_nokey *, CLIENT *);
extern  ypresp_key_val * ypproc_first_2_svc(ypreq_nokey *, struct svc_req *);
#define YPPROC_NEXT ((u_long)5)
extern  ypresp_key_val * ypproc_next_2(ypreq_key *, CLIENT *);
extern  ypresp_key_val * ypproc_next_2_svc(ypreq_key *, struct svc_req *);
#define YPPROC_XFR ((u_long)6)
extern  ypresp_xfr * ypproc_xfr_2(ypreq_xfr *, CLIENT *);
extern  ypresp_xfr * ypproc_xfr_2_svc(ypreq_xfr *, struct svc_req *);
#define YPPROC_CLEAR ((u_long)7)
extern  void * ypproc_clear_2(void *, CLIENT *);
extern  void * ypproc_clear_2_svc(void *, struct svc_req *);
#define YPPROC_ALL ((u_long)8)
extern  ypresp_all * ypproc_all_2(ypreq_nokey *, CLIENT *);
extern  ypresp_all * ypproc_all_2_svc(ypreq_nokey *, struct svc_req *);
#define YPPROC_MASTER ((u_long)9)
extern  ypresp_master * ypproc_master_2(ypreq_nokey *, CLIENT *);
extern  ypresp_master * ypproc_master_2_svc(ypreq_nokey *, struct svc_req *);
#define YPPROC_ORDER ((u_long)10)
extern  ypresp_order * ypproc_order_2(ypreq_nokey *, CLIENT *);
extern  ypresp_order * ypproc_order_2_svc(ypreq_nokey *, struct svc_req *);
#define YPPROC_MAPLIST ((u_long)11)
extern  ypresp_maplist * ypproc_maplist_2(domainname *, CLIENT *);
extern  ypresp_maplist * ypproc_maplist_2_svc(domainname *, struct svc_req *);

#else /* Old Style C */
#define YPPROC_NULL ((u_long)0)
extern  void * ypproc_null_2();
extern  void * ypproc_null_2_svc();
#define YPPROC_DOMAIN ((u_long)1)
extern  bool_t * ypproc_domain_2();
extern  bool_t * ypproc_domain_2_svc();
#define YPPROC_DOMAIN_NONACK ((u_long)2)
extern  bool_t * ypproc_domain_nonack_2();
extern  bool_t * ypproc_domain_nonack_2_svc();
#define YPPROC_MATCH ((u_long)3)
extern  ypresp_val * ypproc_match_2();
extern  ypresp_val * ypproc_match_2_svc();
#define YPPROC_FIRST ((u_long)4)
extern  ypresp_key_val * ypproc_first_2();
extern  ypresp_key_val * ypproc_first_2_svc();
#define YPPROC_NEXT ((u_long)5)
extern  ypresp_key_val * ypproc_next_2();
extern  ypresp_key_val * ypproc_next_2_svc();
#define YPPROC_XFR ((u_long)6)
extern  ypresp_xfr * ypproc_xfr_2();
extern  ypresp_xfr * ypproc_xfr_2_svc();
#define YPPROC_CLEAR ((u_long)7)
extern  void * ypproc_clear_2();
extern  void * ypproc_clear_2_svc();
#define YPPROC_ALL ((u_long)8)
extern  ypresp_all * ypproc_all_2();
extern  ypresp_all * ypproc_all_2_svc();
#define YPPROC_MASTER ((u_long)9)
extern  ypresp_master * ypproc_master_2();
extern  ypresp_master * ypproc_master_2_svc();
#define YPPROC_ORDER ((u_long)10)
extern  ypresp_order * ypproc_order_2();
extern  ypresp_order * ypproc_order_2_svc();
#define YPPROC_MAPLIST ((u_long)11)
extern  ypresp_maplist * ypproc_maplist_2();
extern  ypresp_maplist * ypproc_maplist_2_svc();
#endif /* Old Style C */

#define YPPUSH_XFRRESPPROG ((u_long)0x40000000)
#define YPPUSH_XFRRESPVERS ((u_long)1)

#ifdef __cplusplus
#define YPPUSHPROC_NULL ((u_long)0)
extern "C" void * yppushproc_null_1(void *, CLIENT *);
extern "C" void * yppushproc_null_1_svc(void *, struct svc_req *);
#define YPPUSHPROC_XFRRESP ((u_long)1)
extern "C" yppushresp_xfr * yppushproc_xfrresp_1(void *, CLIENT *);
extern "C" yppushresp_xfr * yppushproc_xfrresp_1_svc(void *, struct svc_req *);

#elif __STDC__
#define YPPUSHPROC_NULL ((u_long)0)
extern  void * yppushproc_null_1(void *, CLIENT *);
extern  void * yppushproc_null_1_svc(void *, struct svc_req *);
#define YPPUSHPROC_XFRRESP ((u_long)1)
extern  yppushresp_xfr * yppushproc_xfrresp_1(void *, CLIENT *);
extern  yppushresp_xfr * yppushproc_xfrresp_1_svc(void *, struct svc_req *);

#else /* Old Style C */
#define YPPUSHPROC_NULL ((u_long)0)
extern  void * yppushproc_null_1();
extern  void * yppushproc_null_1_svc();
#define YPPUSHPROC_XFRRESP ((u_long)1)
extern  yppushresp_xfr * yppushproc_xfrresp_1();
extern  yppushresp_xfr * yppushproc_xfrresp_1_svc();
#endif /* Old Style C */

#define YPBINDPROG ((u_long)100007)
#define YPBINDVERS ((u_long)2)

#ifdef __cplusplus
#define YPBINDPROC_NULL ((u_long)0)
extern "C" void * ypbindproc_null_2(void *, CLIENT *);
extern "C" void * ypbindproc_null_2_svc(void *, struct svc_req *);
#define YPBINDPROC_DOMAIN ((u_long)1)
extern "C" ypbind_resp * ypbindproc_domain_2(domainname *, CLIENT *);
extern "C" ypbind_resp * ypbindproc_domain_2_svc(domainname *, struct svc_req *);
#define YPBINDPROC_SETDOM ((u_long)2)
extern "C" void * ypbindproc_setdom_2(ypbind_setdom *, CLIENT *);
extern "C" void * ypbindproc_setdom_2_svc(ypbind_setdom *, struct svc_req *);

#elif __STDC__
#define YPBINDPROC_NULL ((u_long)0)
extern  void * ypbindproc_null_2(void *, CLIENT *);
extern  void * ypbindproc_null_2_svc(void *, struct svc_req *);
#define YPBINDPROC_DOMAIN ((u_long)1)
extern  ypbind_resp * ypbindproc_domain_2(domainname *, CLIENT *);
extern  ypbind_resp * ypbindproc_domain_2_svc(domainname *, struct svc_req *);
#define YPBINDPROC_SETDOM ((u_long)2)
extern  void * ypbindproc_setdom_2(ypbind_setdom *, CLIENT *);
extern  void * ypbindproc_setdom_2_svc(ypbind_setdom *, struct svc_req *);

#else /* Old Style C */
#define YPBINDPROC_NULL ((u_long)0)
extern  void * ypbindproc_null_2();
extern  void * ypbindproc_null_2_svc();
#define YPBINDPROC_DOMAIN ((u_long)1)
extern  ypbind_resp * ypbindproc_domain_2();
extern  ypbind_resp * ypbindproc_domain_2_svc();
#define YPBINDPROC_SETDOM ((u_long)2)
extern  void * ypbindproc_setdom_2();
extern  void * ypbindproc_setdom_2_svc();
#endif /* Old Style C */

#endif /* !_YP_H_RPCGEN */
