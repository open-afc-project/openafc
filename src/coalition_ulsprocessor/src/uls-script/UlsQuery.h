/*
 *      This class represents a query. Queries can come in several forms, such as String queries,
 * Numeric queries, etc.
 *
 *      Created 26 Feb, 2009 by Erik Halvorson.
 */

#ifndef ULS_QUERY_H
#define ULS_QUERY_H

struct UlsQuery {
        enum { GreaterThan, LessThan, EqualTo } QuerySign;

        struct UlsQuery *next;
        QuerySign sgn;
        int field;
};

#endif
