#	$OpenBSD: Makefile,v 1.13 2016/05/01 00:32:37 jmatthew Exp $

PROG=		ldapd
MAN=		ldapd.8 ldapd.conf.5
SRCS=		ber.c log.c control.c \
		util.c ldapd.c ldape.c conn.c attributes.c namespace.c \
		btree.c filter.c search.c parse.y \
		auth.c modify.c index.c evbuffer_tls.c \
		validate.c uuid.c schema.c imsgev.c syntax.c matching.c

LDADD=		-ltls -levent -lz -lutil
DPADD=		${LIBEVENT} ${LIBTLS} ${LIBCRYPTO} ${LIBSSL} ${LIBZ} ${LIBUTIL}
CFLAGS+=	-I${.CURDIR} -g
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CLEANFILES+=	y.tab.h parse.c

SCHEMA_FILES=	core.schema \
		inetorgperson.schema \
		nis.schema

distribution:
	for i in ${SCHEMA_FILES}; do \
		${INSTALL} -C -o root -g wheel -m 0644 ${.CURDIR}/schema/$$i ${DESTDIR}/etc/ldap/; \
	done

.include <bsd.prog.mk>
