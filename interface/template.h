#ifndef INTERFACE_IF_H
#define INTERFACE_IF_H

/*************************************************************************************************************
 * This header file provides the interface functions required for higher level api's. The integrator
 * must provide the implementations specified in the structs.
 ************************************************************************************************************* /

/* ================================================ INCLUDES =============================================== */
/* ================================================= MACROS ================================================ */
/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct
{
    void (*func1)(void);
    void (*func2)(void);
    void (*func3)(void);
} interface_t;

/* ======================================= EXTERN GLOBAL VARIABLES ========================================= */

#endif /* INTERFACE_IF_H */
