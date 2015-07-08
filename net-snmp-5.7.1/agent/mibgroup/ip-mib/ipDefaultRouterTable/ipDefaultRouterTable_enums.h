/*
 * Note: this file originally auto-generated by mib2c using
 *  : generic-table-enums.m2c 12526 2005-07-15 22:41:16Z rstory $
 *
 * $Id:$
 */
#ifndef IPDEFAULTROUTERTABLE_ENUMS_H
#define IPDEFAULTROUTERTABLE_ENUMS_H

#ifdef __cplusplus
extern          "C" {
#endif

    /*
     * NOTES on enums
     * ==============
     *
     * Value Mapping
     * -------------
     * If the values for your data type don't exactly match the
     * possible values defined by the mib, you should map them
     * below. For example, a boolean flag (1/0) is usually represented
     * as a TruthValue in a MIB, which maps to the values (1/2).
     *
     */
/*************************************************************************
 *************************************************************************
 *
 * enum definitions for table ipDefaultRouterTable
 *
 *************************************************************************
 *************************************************************************/

/*************************************************************
 * constants for enums for the MIB node
 * ipDefaultRouterLifetime (UNSIGNED32 / ASN_UNSIGNED)
 *
 * since a Textual Convention may be referenced more than once in a
 * MIB, protect againt redefinitions of the enum values.
 */
#ifndef IPDEFAULTROUTERLIFETIME_ENUMS
#define IPDEFAULTROUTERLIFETIME_ENUMS

#define IPDEFAULTROUTERLIFETIME_MIN  0
#define IPDEFAULTROUTERLIFETIME_MAX  0xFFFFFFFFUL

#endif                          /* IPDEFAULTROUTERLIFETIME_ENUMS */


/*************************************************************
 * constants for enums for the MIB node
 * ipDefaultRouterAddressType (InetAddressType / ASN_INTEGER)
 *
 * since a Textual Convention may be referenced more than once in a
 * MIB, protect againt redefinitions of the enum values.
 */
#ifndef INETADDRESSTYPE_ENUMS
#define INETADDRESSTYPE_ENUMS

#define INETADDRESSTYPE_UNKNOWN  0
#define INETADDRESSTYPE_IPV4  1
#define INETADDRESSTYPE_IPV6  2
#define INETADDRESSTYPE_IPV4Z  3
#define INETADDRESSTYPE_IPV6Z  4
#define INETADDRESSTYPE_DNS  16

#endif                          /* INETADDRESSTYPE_ENUMS */


/*************************************************************
 * constants for enums for the MIB node
 * ipDefaultRouterPreference (INTEGER / ASN_INTEGER)
 *
 * since a Textual Convention may be referenced more than once in a
 * MIB, protect againt redefinitions of the enum values.
 */
#ifndef IPDEFAULTROUTERPREFERENCE_ENUMS
#define IPDEFAULTROUTERPREFERENCE_ENUMS

#define IPDEFAULTROUTERPREFERENCE_RESERVED  -2
#define IPDEFAULTROUTERPREFERENCE_LOW  -1
#define IPDEFAULTROUTERPREFERENCE_MEDIUM  0
#define IPDEFAULTROUTERPREFERENCE_HIGH  1

#endif                          /* IPDEFAULTROUTERPREFERENCE_ENUMS */




#ifdef __cplusplus
}
#endif
#endif                          /* IPDEFAULTROUTERTABLE_ENUMS_H */
